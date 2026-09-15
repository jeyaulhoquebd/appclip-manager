#include "app_uninstaller.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <glib.h>

/**
 * app_uninstaller_validate_pkg_id:
 * Performs strict validation on package identifiers:
 *  - Must be between 1 and 256 characters long.
 *  - Must not begin with a hyphen (prevents CLI flag injection).
 *  - May only contain ASCII letters, numbers, '.', '-', '_', '+', and ':'.
 */
gboolean app_uninstaller_validate_pkg_id(const char *pkg_id)
{
    if (!pkg_id || *pkg_id == '\0') {
        return FALSE;
    }

    size_t len = strlen(pkg_id);
    if (len > 256) {
        return FALSE;
    }

    /* Reject leading dash or special characters */
    if (pkg_id[0] == '-' || pkg_id[0] == '.') {
        return FALSE;
    }

    for (size_t i = 0; i < len; i++) {
        char c = pkg_id[i];
        if (!g_ascii_isalnum(c) && c != '-' && c != '.' && c != '_' && c != '+' && c != ':') {
            return FALSE;
        }
    }

    return TRUE;
}

/**
 * Data structure for async uninstall worker thread.
 */
typedef struct {
    char *pkg_id;
    char *name;
    PackageType type;
    AppUninstallCallback callback;
    gpointer user_data;
    gboolean success;
    char *err_message;
} UninstallJobData;

/**
 * Main-thread idle callback to invoke user's completion handler.
 */
static gboolean uninstall_idle_completion(gpointer data)
{
    UninstallJobData *job = (UninstallJobData *)data;

    if (job->callback) {
        job->callback(job->success, job->err_message, job->user_data);
    }

    g_free(job->pkg_id);
    g_free(job->name);
    g_free(job->err_message);
    g_free(job);

    return G_SOURCE_REMOVE;
}

/**
 * Background worker thread executing the uninstall process.
 */
static gpointer uninstall_worker_thread(gpointer data)
{
    UninstallJobData *job = (UninstallJobData *)data;

    /* Build command argument vector safely */
    char *argv_storage[8];
    int argc = 0;

    if (job->type == PACKAGE_TYPE_APT) {
        /* pkexec apt-get remove -y <package_name> */
        char *pkexec = g_find_program_in_path("pkexec");
        if (!pkexec) {
            job->success = FALSE;
            job->err_message = g_strdup("PolicyKit (pkexec) is not installed. Root privileges could not be requested.");
            g_idle_add(uninstall_idle_completion, job);
            return NULL;
        }
        g_free(pkexec);

        argv_storage[argc++] = "pkexec";
        argv_storage[argc++] = "apt-get";
        argv_storage[argc++] = "remove";
        argv_storage[argc++] = "-y";
        argv_storage[argc++] = job->pkg_id;
        argv_storage[argc++] = NULL;
    } else if (job->type == PACKAGE_TYPE_SNAP) {
        /* pkexec snap remove <package_name> */
        char *pkexec = g_find_program_in_path("pkexec");
        if (!pkexec) {
            job->success = FALSE;
            job->err_message = g_strdup("PolicyKit (pkexec) is not installed. Root privileges could not be requested.");
            g_idle_add(uninstall_idle_completion, job);
            return NULL;
        }
        g_free(pkexec);

        argv_storage[argc++] = "pkexec";
        argv_storage[argc++] = "snap";
        argv_storage[argc++] = "remove";
        argv_storage[argc++] = job->pkg_id;
        argv_storage[argc++] = NULL;
    } else if (job->type == PACKAGE_TYPE_FLATPAK) {
        /* flatpak uninstall -y <app_id> (User/System flatpak does not require pkexec) */
        char *flatpak = g_find_program_in_path("flatpak");
        if (!flatpak) {
            job->success = FALSE;
            job->err_message = g_strdup("Flatpak utility is not installed on this system.");
            g_idle_add(uninstall_idle_completion, job);
            return NULL;
        }
        g_free(flatpak);

        argv_storage[argc++] = "flatpak";
        argv_storage[argc++] = "uninstall";
        argv_storage[argc++] = "-y";
        argv_storage[argc++] = job->pkg_id;
        argv_storage[argc++] = NULL;
    } else {
        /* Desktop entry fallback */
        job->success = FALSE;
        job->err_message = g_strdup("This application is managed via desktop entry file and has no registered package manager.");
        g_idle_add(uninstall_idle_completion, job);
        return NULL;
    }

    /* Execute command via pipe to capture stderr and stdout */
    int pipe_fds[2];
    if (pipe(pipe_fds) < 0) {
        job->success = FALSE;
        job->err_message = g_strdup("System pipe initialization failed.");
        g_idle_add(uninstall_idle_completion, job);
        return NULL;
    }

    pid_t pid = fork();
    if (pid < 0) {
        job->success = FALSE;
        job->err_message = g_strdup("System process fork failed.");
        close(pipe_fds[0]);
        close(pipe_fds[1]);
        g_idle_add(uninstall_idle_completion, job);
        return NULL;
    }

    if (pid == 0) {
        /* Child process: redirect stderr and stdout into pipe */
        close(pipe_fds[0]);
        dup2(pipe_fds[1], STDOUT_FILENO);
        dup2(pipe_fds[1], STDERR_FILENO);
        close(pipe_fds[1]);

        execvp(argv_storage[0], argv_storage);
        _exit(127);
    }

    /* Parent process: read captured diagnostics */
    close(pipe_fds[1]);

    GString *captured_output = g_string_sized_new(2048);
    char buf[1024];
    ssize_t bytes_read;

    while ((bytes_read = read(pipe_fds[0], buf, sizeof(buf))) > 0) {
        g_string_append_len(captured_output, buf, bytes_read);
    }
    close(pipe_fds[0]);

    int wait_status = 0;
    waitpid(pid, &wait_status, 0);

    int exit_code = WIFEXITED(wait_status) ? WEXITSTATUS(wait_status) : -1;

    if (exit_code == 0) {
        job->success = TRUE;
        job->err_message = NULL;
        g_string_free(captured_output, TRUE);
    } else {
        job->success = FALSE;
        if (exit_code == 126 || exit_code == 127) {
            /* pkexec authentication dismissed or user cancelled */
            job->err_message = g_strdup_printf(
                "Authentication cancelled by user or privilege escalation rejected.\n\nOutput:\n%s",
                captured_output->len > 0 ? captured_output->str : "No further output."
            );
        } else {
            job->err_message = g_strdup_printf(
                "Uninstall command failed with exit code %d.\n\nError details:\n%s",
                exit_code,
                captured_output->len > 0 ? captured_output->str : "No error message provided."
            );
        }
        g_string_free(captured_output, TRUE);
    }

    g_idle_add(uninstall_idle_completion, job);
    return NULL;
}

/**
 * app_uninstaller_run_async:
 * Validates package ID, prepares job payload, and spawns worker thread.
 */
void app_uninstaller_run_async(const AppInfo *app, AppUninstallCallback callback, gpointer user_data)
{
    if (!app || !app->pkg_id) {
        if (callback) {
            callback(FALSE, "Invalid application data provided.", user_data);
        }
        return;
    }

    /* Security check: ensure package ID has no forbidden shell/path traversal chars */
    if (!app_uninstaller_validate_pkg_id(app->pkg_id)) {
        if (callback) {
            callback(FALSE, "Security violation: Package identifier contains invalid or unsafe characters.", user_data);
        }
        return;
    }

    UninstallJobData *job = g_new0(UninstallJobData, 1);
    job->pkg_id = g_strdup(app->pkg_id);
    job->name = g_strdup(app->name ? app->name : app->pkg_id);
    job->type = app->type;
    job->callback = callback;
    job->user_data = user_data;

    g_thread_new("app-uninstaller-thread", uninstall_worker_thread, job);
}
