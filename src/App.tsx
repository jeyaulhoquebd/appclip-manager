import React, { useState, useEffect, useMemo } from 'react';
import {
  Search,
  RefreshCw,
  Trash2,
  Info,
  ExternalLink,
  Code2,
  Terminal,
  Download,
  CheckCircle2,
  AlertTriangle,
  X,
  Package,
  ShieldCheck,
  Copy,
  Check,
  HardDrive,
  Clock,
  Pin,
  FileText,
  Image as ImageIcon,
  Save,
  Database,
  Monitor,
  Filter,
  Plus,
  Palette,
  ArrowRight,
  ArrowUpCircle,
  Sparkles,
  Settings as SettingsIcon,
  Power
} from 'lucide-react';
import { sourceFiles, SourceFile } from './sourceCode';
import { IconShowcase } from './components/IconShowcase';

/* --- Type Definitions --- */

interface AppItem {
  id: string;
  name: string;
  pkgId: string;
  type: 'APT' | 'Snap' | 'Flatpak' | 'Desktop Entry';
  size: string;
  iconName: string;
  comment: string;
  version: string;
  exec: string;
}

interface ClipItem {
  id: number;
  type: 'text' | 'image';
  content: string; // text body or image data/url
  preview: string;
  hash: string;
  charCount: number;
  fileSizeStr: string;
  dimensions?: string;
  pinned: boolean;
  timestamp: number; // unix timestamp
}

export interface UpdateItem {
  id: string;
  packageName: string;
  displayName: string;
  currentVersion: string;
  newVersion: string;
  iconName: string;
  downloadSize: string;
  type: 'APT' | 'Snap' | 'Flatpak';
  description: string;
}

const INITIAL_UPDATES: UpdateItem[] = [
  {
    id: 'apt-firefox',
    packageName: 'firefox',
    displayName: 'Firefox Web Browser',
    currentVersion: '128.0.3',
    newVersion: '130.0.1',
    iconName: 'firefox',
    downloadSize: '68.4 MB',
    type: 'APT',
    description: 'Security & performance upgrades, improved tab navigation and CSS support'
  },
  {
    id: 'snap-code',
    packageName: 'code',
    displayName: 'Visual Studio Code',
    currentVersion: '1.92.2',
    newVersion: '1.93.0',
    iconName: 'vscode',
    downloadSize: '112.5 MB',
    type: 'Snap',
    description: 'Python terminal debugger refresh, modern TypeScript 5.6 compatibility'
  },
  {
    id: 'flatpak-vlc',
    packageName: 'org.videolan.VLC',
    displayName: 'VLC Media Player',
    currentVersion: '3.0.20',
    newVersion: '3.0.21',
    iconName: 'vlc',
    downloadSize: '34.2 MB',
    type: 'Flatpak',
    description: 'AV1 hardware decoder improvements, adaptive audio streaming sync'
  },
  {
    id: 'apt-gimp',
    packageName: 'gimp',
    displayName: 'GNU Image Manipulation Program',
    currentVersion: '2.10.36',
    newVersion: '2.10.38',
    iconName: 'gimp',
    downloadSize: '24.1 MB',
    type: 'APT',
    description: 'Color profile precision updates, improved PSD and WebP image filters'
  },
  {
    id: 'apt-git',
    packageName: 'git',
    displayName: 'Git Version Control',
    currentVersion: '2.43.0',
    newVersion: '2.43.2',
    iconName: 'git',
    downloadSize: '8.2 MB',
    type: 'APT',
    description: 'Fast-import stability enhancements, index packing performance fixes'
  }
];

const INITIAL_APPS: AppItem[] = [
  {
    id: 'firefox',
    name: 'Firefox Web Browser',
    pkgId: 'firefox',
    type: 'APT',
    size: '248.5 MB',
    iconName: 'firefox',
    comment: 'Browse the World Wide Web safely and swiftly',
    version: '128.0.3',
    exec: 'firefox'
  },
  {
    id: 'code',
    name: 'Visual Studio Code',
    pkgId: 'code',
    type: 'Snap',
    size: '342.1 MB',
    iconName: 'code',
    comment: 'Code editing. Redefined.',
    version: '1.92.2',
    exec: 'code'
  },
  {
    id: 'vlc',
    name: 'VLC Media Player',
    pkgId: 'org.videolan.VLC',
    type: 'Flatpak',
    size: '158.4 MB',
    iconName: 'vlc',
    comment: 'Read, capture, broadcast your multimedia streams',
    version: '3.0.21',
    exec: 'vlc'
  },
  {
    id: 'gimp',
    name: 'GNU Image Manipulation Program',
    pkgId: 'gimp',
    type: 'APT',
    size: '185.0 MB',
    iconName: 'gimp',
    comment: 'Create and manipulate photographic imagery',
    version: '2.10.36',
    exec: 'gimp'
  },
  {
    id: 'obsidian',
    name: 'Obsidian Note Taking',
    pkgId: 'md.obsidian.Obsidian',
    type: 'Flatpak',
    size: '194.2 MB',
    iconName: 'obsidian',
    comment: 'A second brain, for you, forever',
    version: '1.6.7',
    exec: 'obsidian'
  },
  {
    id: 'calculator',
    name: 'GNOME Calculator',
    pkgId: 'gnome-calculator',
    type: 'APT',
    size: '4.8 MB',
    iconName: 'calculator',
    comment: 'Perform arithmetic, scientific or financial calculations',
    version: '44.0',
    exec: 'gnome-calculator'
  },
  {
    id: 'discord',
    name: 'Discord',
    pkgId: 'discord',
    type: 'Snap',
    size: '220.0 MB',
    iconName: 'discord',
    comment: 'All-in-one voice and text chat for gamers',
    version: '0.0.58',
    exec: 'discord'
  }
];

const INITIAL_CLIPS: ClipItem[] = [
  {
    id: 1,
    type: 'text',
    content: 'https://jeyaulhoque.pages.dev/',
    preview: 'https://jeyaulhoque.pages.dev/',
    hash: 'a38f90c17a9e1208',
    charCount: 30,
    fileSizeStr: '30 B',
    pinned: true,
    timestamp: Date.now() - 1000 * 60 * 5 // 5 mins ago
  },
  {
    id: 2,
    type: 'text',
    content: 'sudo apt update && sudo apt install -y libgtk-3-dev libsqlite3-dev',
    preview: 'sudo apt update && sudo apt install -y libgtk-3-dev libsqlite3-dev',
    hash: '8f0b12d5e71c9902',
    charCount: 66,
    fileSizeStr: '66 B',
    pinned: true,
    timestamp: Date.now() - 1000 * 60 * 18 // 18 mins ago
  },
  {
    id: 3,
    type: 'image',
    content: '/logo.png',
    preview: 'AppClip Manager Brand Logo (128x128 PNG)',
    hash: 'e499d1045fc98b12',
    charCount: 0,
    fileSizeStr: '14.2 KB',
    dimensions: '128 × 128',
    pinned: false,
    timestamp: Date.now() - 1000 * 60 * 45 // 45 mins ago
  },
  {
    id: 4,
    type: 'text',
    content: 'SELECT id, type, content, preview, timestamp, pinned FROM clips ORDER BY pinned DESC, timestamp DESC LIMIT 50;',
    preview: 'SELECT id, type, content, preview, timestamp, pinned FROM clips ORDER BY...',
    hash: 'c2459bca4013de8f',
    charCount: 111,
    fileSizeStr: '111 B',
    pinned: false,
    timestamp: Date.now() - 1000 * 60 * 120 // 2 hours ago
  },
  {
    id: 5,
    type: 'text',
    content: 'export XDG_SESSION_TYPE=wayland\nsudo apt install wl-clipboard',
    preview: 'export XDG_SESSION_TYPE=wayland sudo apt install wl-clipboard',
    hash: '57a1b920cd77ea10',
    charCount: 61,
    fileSizeStr: '61 B',
    pinned: false,
    timestamp: Date.now() - 1000 * 60 * 360 // 6 hours ago
  }
];

export function App() {
  /* Active Tab: 'apps' (Part 1) | 'clipboard' (Part 2) | 'updates' (Part 3) | 'source' | 'branding' */
  const [activeTab, setActiveTab] = useState<'apps' | 'clipboard' | 'updates' | 'source' | 'branding'>('apps');

  /* Apps State */
  const [apps, setApps] = useState<AppItem[]>(INITIAL_APPS);
  const [appSearch, setAppSearch] = useState('');
  const [appFilterType, setAppFilterType] = useState<string>('ALL');
  const [isScanning, setIsScanning] = useState(false);
  const [selectedAppToUninstall, setSelectedAppToUninstall] = useState<AppItem | null>(null);
  const [isUninstalling, setIsUninstalling] = useState(false);
  const [uninstallProgress, setUninstallProgress] = useState(0);

  /* Clipboard State */
  const [clips, setClips] = useState<ClipItem[]>(INITIAL_CLIPS);
  const [clipSearch, setClipSearch] = useState('');
  const [clipFilter, setClipFilter] = useState<'all' | 'pinned' | 'text' | 'image'>('all');
  const [newClipInput, setNewClipInput] = useState('');
  const [sessionType, setSessionType] = useState<'x11' | 'wayland'>('x11');
  const [showClearConfirm, setShowClearConfirm] = useState(false);

  /* Updates State (Part 3) */
  const [updates, setUpdates] = useState<UpdateItem[]>(INITIAL_UPDATES);
  const [isCheckingUpdates, setIsCheckingUpdates] = useState(false);
  const [lastUpdateCheckTime, setLastUpdateCheckTime] = useState<number>(Date.now() - 1000 * 60 * 14); // 14 mins ago
  const [updatingRowId, setUpdatingRowId] = useState<string | null>(null);
  const [isUpdatingAll, setIsUpdatingAll] = useState(false);
  const [updateAllProgress, setUpdateAllProgress] = useState<{ current: number; total: number; currentPackage: string }>({
    current: 0,
    total: 0,
    currentPackage: ''
  });

  /* Source Code Viewer State */
  const [selectedFile, setSelectedFile] = useState<SourceFile>(sourceFiles[0]);
  const [copiedCode, setCopiedCode] = useState(false);

  /* About Dialog State */
  const [showAbout, setShowAbout] = useState(false);

  /* Settings Dialog State */
  const [showSettings, setShowSettings] = useState(false);
  const [startOnBoot, setStartOnBoot] = useState(true);
  const [maxHistoryItems, setMaxHistoryItems] = useState(500);
  const [maxHistoryDays, setMaxHistoryDays] = useState(30);

  /* Toast State */
  const [toastMessage, setToastMessage] = useState<string | null>(null);

  const showToast = (msg: string) => {
    setToastMessage(msg);
    setTimeout(() => {
      setToastMessage(null);
    }, 3000);
  };

  /* Updates actions */
  const handleCheckUpdates = () => {
    if (isCheckingUpdates || isUpdatingAll) return;
    setIsCheckingUpdates(true);
    showToast('Checking package repositories (APT, Snap, Flatpak)...');

    setTimeout(() => {
      setIsCheckingUpdates(false);
      setLastUpdateCheckTime(Date.now());
      if (updates.length === 0) {
        setUpdates(INITIAL_UPDATES);
        showToast(`Update check complete: ${INITIAL_UPDATES.length} updates found!`);
      } else {
        showToast(`Update check complete: ${updates.length} updates available.`);
      }
    }, 1400);
  };

  const handleSingleUpdate = (item: UpdateItem) => {
    if (updatingRowId || isUpdatingAll) return;
    setUpdatingRowId(item.id);
    showToast(`Installing update for ${item.displayName}...`);

    setTimeout(() => {
      setUpdates((prev) => prev.filter((u) => u.id !== item.id));
      setUpdatingRowId(null);
      showToast(`✓ Successfully updated ${item.displayName} to v${item.newVersion}!`);
    }, 1500);
  };

  const handleUpdateAll = () => {
    if (updates.length === 0 || isUpdatingAll || isCheckingUpdates) return;
    setIsUpdatingAll(true);
    const total = updates.length;
    let currentIdx = 0;

    const runNextStep = () => {
      if (currentIdx >= total || updates.length === 0) {
        setIsUpdatingAll(false);
        setUpdates([]);
        showToast(`✓ All ${total} updates installed successfully! Your system is up to date.`);
        return;
      }

      const currentItem = updates[0];
      setUpdateAllProgress({
        current: currentIdx + 1,
        total,
        currentPackage: `${currentItem.displayName} (${currentItem.type})`
      });

      setTimeout(() => {
        setUpdates((prev) => prev.slice(1));
        currentIdx++;
        if (currentIdx < total) {
          runNextStep();
        } else {
          setIsUpdatingAll(false);
          showToast(`✓ All ${total} updates installed successfully! Your system is up to date.`);
        }
      }, 1300);
    };

    runNextStep();
  };
  const formatTimeAgo = (ts: number) => {
    const diffSec = Math.floor((Date.now() - ts) / 1000);
    if (diffSec < 60) return 'Just now';
    if (diffSec < 3600) return `${Math.floor(diffSec / 60)}m ago`;
    if (diffSec < 86400) return `${Math.floor(diffSec / 3600)}h ago`;
    return `${Math.floor(diffSec / 86400)}d ago`;
  };

  /* Filtered Apps */
  const filteredApps = useMemo(() => {
    return apps.filter((app) => {
      const matchesSearch =
        app.name.toLowerCase().includes(appSearch.toLowerCase()) ||
        app.pkgId.toLowerCase().includes(appSearch.toLowerCase()) ||
        app.comment.toLowerCase().includes(appSearch.toLowerCase());
      const matchesType = appFilterType === 'ALL' || app.type === appFilterType;
      return matchesSearch && matchesType;
    });
  }, [apps, appSearch, appFilterType]);

  /* Filtered Clips */
  const filteredClips = useMemo(() => {
    let list = clips.filter((c) => {
      const q = clipSearch.toLowerCase();
      const matchesSearch =
        !q ||
        c.preview.toLowerCase().includes(q) ||
        c.content.toLowerCase().includes(q) ||
        c.hash.toLowerCase().includes(q);

      if (!matchesSearch) return false;

      if (clipFilter === 'pinned') return c.pinned;
      if (clipFilter === 'text') return c.type === 'text';
      if (clipFilter === 'image') return c.type === 'image';
      return true;
    });

    // Pinned always on top, then newest first
    return list.sort((a, b) => {
      if (a.pinned && !b.pinned) return -1;
      if (!a.pinned && b.pinned) return 1;
      return b.timestamp - a.timestamp;
    });
  }, [clips, clipSearch, clipFilter]);

  /* Clipboard Actions */
  const handleCopyClipAgain = (clip: ClipItem) => {
    if (clip.type === 'text') {
      if (navigator.clipboard && navigator.clipboard.writeText) {
        navigator.clipboard.writeText(clip.content).catch(() => {});
      }
      showToast('Text copied to clipboard!');
    } else {
      showToast('Image copied to clipboard!');
    }
  };

  const handleTogglePin = (id: number) => {
    setClips((prev) =>
      prev.map((c) => {
        if (c.id === id) {
          const nextState = !c.pinned;
          showToast(nextState ? 'Clip pinned to top' : 'Clip unpinned');
          return { ...c, pinned: nextState };
        }
        return c;
      })
    );
  };

  const handleDeleteClip = (id: number) => {
    setClips((prev) => prev.filter((c) => c.id !== id));
    showToast('Clip deleted from history');
  };

  const handleSaveClipAs = (clip: ClipItem) => {
    if (clip.type === 'text') {
      const blob = new Blob([clip.content], { type: 'text/plain' });
      const url = URL.createObjectURL(blob);
      const a = document.createElement('a');
      a.href = url;
      a.download = `clip_${clip.id}_${Date.now()}.txt`;
      a.click();
      URL.revokeObjectURL(url);
      showToast('Saved text clip to file');
    } else {
      const a = document.createElement('a');
      a.href = clip.content;
      a.download = `clip_${clip.id}_image.png`;
      a.click();
      showToast('Saved image clip to file');
    }
  };

  const handleAddCustomClip = (e: React.FormEvent) => {
    e.preventDefault();
    if (!newClipInput.trim()) return;

    const trimmed = newClipInput.trim();
    // Simple hash
    const fakeHash = Math.random().toString(16).substring(2, 18);
    const newEntry: ClipItem = {
      id: Date.now(),
      type: 'text',
      content: trimmed,
      preview: trimmed.length > 80 ? trimmed.substring(0, 80) + '...' : trimmed,
      hash: fakeHash,
      charCount: trimmed.length,
      fileSizeStr: `${new TextEncoder().encode(trimmed).length} B`,
      pinned: false,
      timestamp: Date.now()
    };

    setClips((prev) => [newEntry, ...prev]);
    setNewClipInput('');
    showToast('New text clip captured to SQLite history');
  };

  const handleAddSampleImageClip = () => {
    const fakeHash = Math.random().toString(16).substring(2, 18);
    const newImage: ClipItem = {
      id: Date.now(),
      type: 'image',
      content: '/logo.png',
      preview: 'Captured Screenshot / Clipboard Image (128x128)',
      hash: fakeHash,
      charCount: 0,
      fileSizeStr: '16.8 KB',
      dimensions: '128 × 128',
      pinned: false,
      timestamp: Date.now()
    };
    setClips((prev) => [newImage, ...prev]);
    showToast('Image thumbnail saved to disk & indexed in SQLite');
  };

  const handleClearHistory = () => {
    // Keep pinned items
    setClips((prev) => prev.filter((c) => c.pinned));
    setShowClearConfirm(false);
    showToast('Cleared non-pinned clipboard history');
  };

  /* Refresh Apps Action */
  const handleRefreshApps = () => {
    setIsScanning(true);
    showToast('Scanning APT, Snap, Flatpak, and Desktop entries...');
    setTimeout(() => {
      setIsScanning(false);
      showToast('Applications refreshed: 7 installed packages detected');
    }, 1200);
  };

  /* Confirm Uninstall Action */
  const handlePerformUninstall = () => {
    if (!selectedAppToUninstall) return;
    setIsUninstalling(true);
    setUninstallProgress(20);

    const timer1 = setTimeout(() => setUninstallProgress(60), 600);
    const timer2 = setTimeout(() => {
      setUninstallProgress(100);
      setApps((prev) => prev.filter((a) => a.id !== selectedAppToUninstall.id));
      setIsUninstalling(false);
      const name = selectedAppToUninstall.name;
      setSelectedAppToUninstall(null);
      showToast(`Successfully uninstalled ${name}`);
    }, 1400);

    return () => {
      clearTimeout(timer1);
      clearTimeout(timer2);
    };
  };

  const copySourceCode = () => {
    if (navigator.clipboard && navigator.clipboard.writeText) {
      navigator.clipboard.writeText(selectedFile.content);
      setCopiedCode(true);
      setTimeout(() => setCopiedCode(false), 2000);
    }
  };

  return (
    <div className="min-h-screen bg-slate-900 text-slate-100 flex flex-col font-sans selection:bg-blue-600 selection:text-white">
      {/* Top Application Desktop Banner */}
      <header className="bg-slate-950 border-b border-slate-800 px-4 py-2.5 flex items-center justify-between shadow-sm">
        <div className="flex items-center gap-3">
          <div className="flex items-center gap-1.5 mr-2">
            <span className="w-3 h-3 rounded-full bg-red-500/80 inline-block"></span>
            <span className="w-3 h-3 rounded-full bg-amber-500/80 inline-block"></span>
            <span className="w-3 h-3 rounded-full bg-emerald-500/80 inline-block"></span>
          </div>

          <div className="flex items-center gap-2">
            <img
              src="/icons/appclip-manager-32x32.png"
              alt="AppClip Logo"
              className="w-7 h-7 rounded object-contain shadow-sm cursor-pointer hover:scale-105 transition-transform"
              onClick={() => setActiveTab('branding')}
              title="Click to view App Icon & Visual Identity"
              onError={(e) => {
                (e.target as HTMLImageElement).src = '/logo.png';
              }}
            />
            <div>
              <h1 className="text-sm font-bold tracking-tight text-white flex items-center gap-2">
                AppClip Manager
                <span className="text-[10px] px-1.5 py-0.5 rounded bg-blue-500/20 text-blue-400 font-mono border border-blue-500/30">
                  GTK3 + SQLite3 v2.0
                </span>
              </h1>
              <p className="text-xs text-slate-400">
                {activeTab === 'apps' && 'Installed Applications Manager (APT, Snap, Flatpak)'}
                {activeTab === 'clipboard' && 'Persistent Clipboard History Manager with SQLite & Thumbnails'}
                {activeTab === 'source' && 'C Architecture, Makefile, Meson & Source Code'}
                {activeTab === 'branding' && 'Modern Ubuntu App Icon & Multi-Resolution FreeDesktop Branding'}
              </p>
            </div>
          </div>
        </div>

        {/* Global Action Header Buttons */}
        <div className="flex items-center gap-2">
          {activeTab === 'apps' && (
            <button
              onClick={handleRefreshApps}
              disabled={isScanning}
              className="px-3 py-1.5 text-xs font-medium bg-slate-800 hover:bg-slate-700 text-slate-200 rounded-md border border-slate-700 transition flex items-center gap-1.5 disabled:opacity-50"
              title="Rescan installed packages (APT, Snap, Flatpak)"
            >
              <RefreshCw className={`w-3.5 h-3.5 ${isScanning ? 'animate-spin text-blue-400' : ''}`} />
              <span>Refresh</span>
            </button>
          )}

          {activeTab === 'clipboard' && (
            <button
              onClick={() => {
                showToast('Reloaded SQLite clips');
              }}
              className="px-3 py-1.5 text-xs font-medium bg-slate-800 hover:bg-slate-700 text-slate-200 rounded-md border border-slate-700 transition flex items-center gap-1.5"
              title="Reload SQLite clipboard database"
            >
              <RefreshCw className="w-3.5 h-3.5 text-blue-400" />
              <span>Reload DB</span>
            </button>
          )}

          {activeTab === 'updates' && (
            <button
              onClick={handleCheckUpdates}
              disabled={isCheckingUpdates || isUpdatingAll}
              className="px-3 py-1.5 text-xs font-medium bg-slate-800 hover:bg-slate-700 disabled:opacity-50 text-slate-200 rounded-md border border-slate-700 transition flex items-center gap-1.5"
              title="Query APT, Snap, and Flatpak repositories for updates"
            >
              <RefreshCw className={`w-3.5 h-3.5 text-blue-400 ${isCheckingUpdates ? 'animate-spin' : ''}`} />
              <span>{isCheckingUpdates ? 'Checking...' : 'Check Updates'}</span>
            </button>
          )}

          <button
            onClick={() => setActiveTab('branding')}
            className={`px-3 py-1.5 text-xs font-medium rounded-md border transition flex items-center gap-1.5 ${
              activeTab === 'branding'
                ? 'bg-orange-500/20 text-orange-400 border-orange-500/40'
                : 'bg-slate-800 hover:bg-slate-700 text-slate-300 border-slate-700'
            }`}
            title="Inspect App Icon (512px to 16px)"
          >
            <Palette className="w-3.5 h-3.5 text-orange-400" />
            <span className="hidden sm:inline">Icon Studio</span>
          </button>

          <a
            href="/appclip-manager-v3.0.tar.gz"
            download
            className="px-3 py-1.5 text-xs font-medium bg-blue-600 hover:bg-blue-500 text-white rounded-md transition flex items-center gap-1.5 shadow-sm"
            title="Download compiled C source & build files with Update Checker"
          >
            <Download className="w-3.5 h-3.5" />
            <span className="hidden sm:inline">Download C Tarball v3.0</span>
          </a>

          <button
            onClick={() => setShowSettings(true)}
            className="px-2.5 py-1.5 text-xs font-medium bg-slate-800 hover:bg-slate-700 text-slate-300 rounded-md border border-slate-700 transition flex items-center gap-1.5"
            title="Configure AppClip Manager Settings (Autostart & Retention)"
          >
            <SettingsIcon className="w-3.5 h-3.5 text-blue-400" />
            <span className="hidden sm:inline">Settings</span>
          </button>

          <button
            onClick={() => setShowAbout(true)}
            className="px-2.5 py-1.5 text-xs font-medium bg-slate-800 hover:bg-slate-700 text-slate-300 rounded-md border border-slate-700 transition flex items-center gap-1"
            title="About AppClip Manager"
          >
            <Info className="w-3.5 h-3.5 text-amber-400" />
            <span className="hidden sm:inline">About</span>
          </button>
        </div>
      </header>

      {/* GTK-style Notebook Tabs Bar */}
      <div className="bg-slate-950/70 border-b border-slate-800 px-4 flex items-center gap-2 select-none overflow-x-auto">
        <button
          onClick={() => setActiveTab('apps')}
          className={`flex items-center gap-2 px-4 py-2.5 text-xs font-semibold border-b-2 transition ${
            activeTab === 'apps'
              ? 'border-blue-500 text-blue-400 bg-slate-900/60'
              : 'border-transparent text-slate-400 hover:text-slate-200 hover:bg-slate-900/30'
          }`}
        >
          <Package className="w-4 h-4" />
          <span>Part 1: Installed Applications</span>
          <span className="text-[10px] px-1.5 py-0.2 rounded-full bg-slate-800 text-slate-400 border border-slate-700">
            {apps.length}
          </span>
        </button>

        <button
          onClick={() => setActiveTab('clipboard')}
          className={`flex items-center gap-2 px-4 py-2.5 text-xs font-semibold border-b-2 transition ${
            activeTab === 'clipboard'
              ? 'border-blue-500 text-blue-400 bg-slate-900/60'
              : 'border-transparent text-slate-400 hover:text-slate-200 hover:bg-slate-900/30'
          }`}
        >
          <FileText className="w-4 h-4" />
          <span>Part 2: Clipboard History</span>
          <span className="text-[10px] px-1.5 py-0.2 rounded-full bg-blue-900/50 text-blue-300 border border-blue-800">
            {clips.length}
          </span>
        </button>

        <button
          onClick={() => setActiveTab('updates')}
          className={`flex items-center gap-2 px-4 py-2.5 text-xs font-semibold border-b-2 transition ${
            activeTab === 'updates'
              ? 'border-blue-500 text-blue-400 bg-slate-900/60'
              : 'border-transparent text-slate-400 hover:text-slate-200 hover:bg-slate-900/30'
          }`}
        >
          <ArrowUpCircle className="w-4 h-4 text-emerald-400" />
          <span>Part 3: App Updates</span>
          <span
            className={`text-[10px] px-1.5 py-0.2 rounded-full border ${
              updates.length > 0
                ? 'bg-emerald-950 text-emerald-300 border-emerald-800 font-bold'
                : 'bg-slate-800 text-slate-400 border-slate-700'
            }`}
          >
            {updates.length > 0 ? updates.length : '✓'}
          </span>
        </button>

        <button
          onClick={() => setActiveTab('source')}
          className={`flex items-center gap-2 px-4 py-2.5 text-xs font-semibold border-b-2 transition ${
            activeTab === 'source'
              ? 'border-blue-500 text-blue-400 bg-slate-900/60'
              : 'border-transparent text-slate-400 hover:text-slate-200 hover:bg-slate-900/30'
          }`}
        >
          <Code2 className="w-4 h-4" />
          <span>C Code & Architecture</span>
          <span className="text-[10px] px-1.5 py-0.2 rounded-full bg-slate-800 text-emerald-400 border border-slate-700">
            {sourceFiles.length} Files
          </span>
        </button>

        <button
          onClick={() => setActiveTab('branding')}
          className={`flex items-center gap-2 px-4 py-2.5 text-xs font-semibold border-b-2 transition ${
            activeTab === 'branding'
              ? 'border-orange-500 text-orange-400 bg-slate-900/60'
              : 'border-transparent text-slate-400 hover:text-slate-200 hover:bg-slate-900/30'
          }`}
        >
          <Palette className="w-4 h-4" />
          <span>Part 3: App Icon & Branding</span>
          <span className="text-[10px] px-1.5 py-0.2 rounded-full bg-orange-950/60 text-orange-400 border border-orange-800">
            512px to 16px
          </span>
        </button>
      </div>

      {/* Main Content Area */}
      <main className="flex-1 p-4 md:p-6 max-w-7xl mx-auto w-full flex flex-col gap-4">
        {/* ========================================================================= */}
        {/* TAB 1: INSTALLED APPLICATIONS (PART 1) */}
        {/* ========================================================================= */}
        {activeTab === 'apps' && (
          <div className="flex flex-col gap-4">
            {/* Search & Quick Filter Controls */}
            <div className="bg-slate-950 p-4 rounded-xl border border-slate-800 shadow-sm flex flex-col md:flex-row gap-3 items-center justify-between">
              <div className="relative w-full md:w-96">
                <Search className="w-4 h-4 text-slate-400 absolute left-3 top-1/2 -translate-y-1/2" />
                <input
                  type="text"
                  placeholder="Search installed apps by name, ID, or description..."
                  value={appSearch}
                  onChange={(e) => setAppSearch(e.target.value)}
                  className="w-full pl-9 pr-8 py-2 bg-slate-900 border border-slate-700 rounded-lg text-xs text-white placeholder-slate-500 focus:outline-none focus:border-blue-500 focus:ring-1 focus:ring-blue-500 transition"
                />
                {appSearch && (
                  <button
                    onClick={() => setAppSearch('')}
                    className="absolute right-2.5 top-1/2 -translate-y-1/2 text-slate-400 hover:text-white"
                  >
                    <X className="w-3.5 h-3.5" />
                  </button>
                )}
              </div>

              {/* Package Filter Pills */}
              <div className="flex items-center gap-1.5 w-full md:w-auto overflow-x-auto pb-1 md:pb-0">
                {(['ALL', 'APT', 'Snap', 'Flatpak', 'Desktop Entry'] as const).map((t) => (
                  <button
                    key={t}
                    onClick={() => setAppFilterType(t)}
                    className={`px-3 py-1.5 text-xs rounded-lg font-medium transition ${
                      appFilterType === t
                        ? 'bg-blue-600 text-white shadow-sm'
                        : 'bg-slate-900 text-slate-400 hover:text-slate-200 border border-slate-800'
                    }`}
                  >
                    {t}
                  </button>
                ))}
              </div>
            </div>

            {/* List of Installed Applications (GTK3 GtkListBox representation) */}
            <div className="bg-slate-950 rounded-xl border border-slate-800 shadow-sm overflow-hidden flex flex-col">
              <div className="px-4 py-3 bg-slate-900/50 border-b border-slate-800 flex items-center justify-between text-xs text-slate-400">
                <div className="flex items-center gap-2">
                  <span className="font-semibold text-slate-300">Installed Packages</span>
                  <span>({filteredApps.length} shown)</span>
                </div>
                <div className="flex items-center gap-4">
                  <span className="hidden sm:inline">Installed Size</span>
                  <span>Action</span>
                </div>
              </div>

              <div className="divide-y divide-slate-800/60 max-h-[520px] overflow-y-auto">
                {filteredApps.length === 0 ? (
                  <div className="py-12 text-center text-slate-400 text-sm">
                    No applications match the search query "{appSearch}".
                  </div>
                ) : (
                  filteredApps.map((app) => (
                    <div
                      key={app.id}
                      className="p-4 hover:bg-slate-900/50 transition flex items-center justify-between gap-4 group"
                    >
                      <div className="flex items-center gap-3.5 min-w-0">
                        {/* App Icon */}
                        <div className="w-10 h-10 rounded-lg bg-slate-800 border border-slate-700 flex items-center justify-center shrink-0 shadow-sm">
                          <Package className="w-5 h-5 text-blue-400" />
                        </div>

                        {/* Title and Metadata */}
                        <div className="min-w-0">
                          <div className="flex items-center gap-2 flex-wrap">
                            <span className="font-semibold text-sm text-white truncate">{app.name}</span>

                            {/* Badge */}
                            <span
                              className={`text-[10px] font-bold px-2 py-0.5 rounded-full border ${
                                app.type === 'APT'
                                  ? 'bg-blue-950 text-blue-300 border-blue-800'
                                  : app.type === 'Snap'
                                  ? 'bg-orange-950 text-orange-300 border-orange-800'
                                  : app.type === 'Flatpak'
                                  ? 'bg-indigo-950 text-indigo-300 border-indigo-800'
                                  : 'bg-slate-800 text-slate-300 border-slate-700'
                              }`}
                            >
                              {app.type}
                            </span>

                            {app.version && (
                              <span className="text-[11px] text-slate-500 font-mono">v{app.version}</span>
                            )}
                          </div>

                          <p className="text-xs text-slate-400 truncate mt-0.5">{app.comment}</p>
                          <div className="flex items-center gap-3 text-[11px] text-slate-500 font-mono mt-1">
                            <span>pkg: {app.pkgId}</span>
                            <span>•</span>
                            <span>exec: {app.exec}</span>
                          </div>
                        </div>
                      </div>

                      {/* Right: Size & Uninstall Button */}
                      <div className="flex items-center gap-4 shrink-0">
                        <span className="text-xs text-slate-400 font-mono hidden sm:inline">{app.size}</span>

                        <button
                          onClick={() => setSelectedAppToUninstall(app)}
                          className="px-3 py-1.5 text-xs font-medium bg-red-950/40 hover:bg-red-900/60 text-red-300 border border-red-800/60 rounded-md transition flex items-center gap-1.5 group-hover:border-red-600"
                        >
                          <Trash2 className="w-3.5 h-3.5 text-red-400" />
                          <span className="hidden sm:inline">Uninstall</span>
                        </button>
                      </div>
                    </div>
                  ))
                )}
              </div>

              {/* Status Bar */}
              <div className="px-4 py-2 bg-slate-950 border-t border-slate-800 flex items-center justify-between text-xs text-slate-500">
                <span>Ready • Total size calculated from dpkg / flatpak</span>
                <span>AppClip Manager • by Jeyaul Hoque</span>
              </div>
            </div>
          </div>
        )}

        {/* ========================================================================= */}
        {/* TAB 2: CLIPBOARD HISTORY (PART 2) */}
        {/* ========================================================================= */}
        {activeTab === 'clipboard' && (
          <div className="flex flex-col gap-4">
            {/* Wayland vs X11 Session Switcher & Notice Bar */}
            <div className="bg-slate-950 p-4 rounded-xl border border-slate-800 shadow-sm flex flex-col md:flex-row items-start md:items-center justify-between gap-3">
              <div className="flex items-center gap-3">
                <div className="p-2 rounded-lg bg-blue-950/50 border border-blue-800/50 text-blue-400">
                  <Monitor className="w-5 h-5" />
                </div>
                <div>
                  <div className="flex items-center gap-2">
                    <span className="text-xs font-bold text-slate-200">Runtime Session Monitor:</span>
                    <span className="text-[11px] font-mono px-2 py-0.5 rounded bg-slate-900 text-blue-400 border border-slate-700">
                      $XDG_SESSION_TYPE={sessionType}
                    </span>
                  </div>
                  <p className="text-xs text-slate-400 mt-0.5">
                    {sessionType === 'x11'
                      ? 'Listening via GtkClipboard owner-change signal on GDK_SELECTION_CLIPBOARD.'
                      : 'Polling clipboard using wl-paste background sub-processes via wl-clipboard.'}
                  </p>
                </div>
              </div>

              {/* Switch Session Mode button */}
              <div className="flex items-center gap-2 w-full md:w-auto">
                <button
                  onClick={() => {
                    const next = sessionType === 'x11' ? 'wayland' : 'x11';
                    setSessionType(next);
                    showToast(`Switched monitoring mode to ${next.toUpperCase()}`);
                  }}
                  className="px-3 py-1.5 text-xs font-medium bg-slate-900 hover:bg-slate-800 text-slate-300 rounded-lg border border-slate-700 transition flex items-center gap-1.5"
                >
                  <span>Toggle to {sessionType === 'x11' ? 'Wayland' : 'X11'}</span>
                </button>
              </div>
            </div>

            {/* Wayland package info alert (if in wayland mode) */}
            {sessionType === 'wayland' && (
              <div className="bg-amber-950/40 border border-amber-800/60 p-3 rounded-lg flex items-center justify-between gap-3 text-xs text-amber-200">
                <div className="flex items-center gap-2">
                  <AlertTriangle className="w-4 h-4 text-amber-400 shrink-0" />
                  <span>
                    Wayland clipboard monitoring requires the <strong>wl-clipboard</strong> package (provides{' '}
                    <code className="bg-amber-950 px-1 py-0.5 rounded border border-amber-800">wl-paste</code>).
                  </span>
                </div>
                <code className="bg-slate-950 px-2.5 py-1 rounded text-slate-300 font-mono text-[11px] border border-slate-800 hidden sm:inline">
                  sudo apt install wl-clipboard
                </code>
              </div>
            )}

            {/* Quick Capture / Test Playground Form */}
            <div className="bg-slate-950 p-4 rounded-xl border border-slate-800 shadow-sm flex flex-col gap-3">
              <span className="text-xs font-bold text-slate-300 flex items-center gap-1.5">
                <Plus className="w-3.5 h-3.5 text-blue-400" />
                <span>Simulate Clipboard Copy (Adds directly to SQLite database):</span>
              </span>

              <form onSubmit={handleAddCustomClip} className="flex gap-2">
                <input
                  type="text"
                  placeholder="Type any text or command here to copy to clipboard history..."
                  value={newClipInput}
                  onChange={(e) => setNewClipInput(e.target.value)}
                  className="flex-1 px-3 py-2 bg-slate-900 border border-slate-700 rounded-lg text-xs text-white placeholder-slate-500 focus:outline-none focus:border-blue-500 focus:ring-1 focus:ring-blue-500 transition"
                />
                <button
                  type="submit"
                  className="px-4 py-2 text-xs font-semibold bg-blue-600 hover:bg-blue-500 text-white rounded-lg transition flex items-center gap-1.5 shadow-sm shrink-0"
                >
                  <Copy className="w-3.5 h-3.5" />
                  <span>Capture Text</span>
                </button>
                <button
                  type="button"
                  onClick={handleAddSampleImageClip}
                  className="px-3.5 py-2 text-xs font-medium bg-slate-800 hover:bg-slate-700 text-slate-200 rounded-lg border border-slate-700 transition flex items-center gap-1.5 shrink-0"
                  title="Simulate saving screenshot image to disk and indexing in SQLite"
                >
                  <ImageIcon className="w-3.5 h-3.5 text-emerald-400" />
                  <span className="hidden sm:inline">Capture Image</span>
                </button>
              </form>
            </div>

            {/* Clipboard Search and Controls Bar */}
            <div className="bg-slate-950 p-4 rounded-xl border border-slate-800 shadow-sm flex flex-col md:flex-row gap-3 items-center justify-between">
              {/* Search */}
              <div className="relative w-full md:w-80">
                <Search className="w-4 h-4 text-slate-400 absolute left-3 top-1/2 -translate-y-1/2" />
                <input
                  type="text"
                  placeholder="Search clipboard by text or hash..."
                  value={clipSearch}
                  onChange={(e) => setClipSearch(e.target.value)}
                  className="w-full pl-9 pr-8 py-2 bg-slate-900 border border-slate-700 rounded-lg text-xs text-white placeholder-slate-500 focus:outline-none focus:border-blue-500 focus:ring-1 focus:ring-blue-500 transition"
                />
                {clipSearch && (
                  <button
                    onClick={() => setClipSearch('')}
                    className="absolute right-2.5 top-1/2 -translate-y-1/2 text-slate-400 hover:text-white"
                  >
                    <X className="w-3.5 h-3.5" />
                  </button>
                )}
              </div>

              {/* Filter pills & Clear Button */}
              <div className="flex items-center gap-2 w-full md:w-auto justify-between md:justify-end">
                <div className="flex items-center gap-1 bg-slate-900 p-1 rounded-lg border border-slate-800">
                  <button
                    onClick={() => setClipFilter('all')}
                    className={`px-2.5 py-1 text-xs rounded font-medium transition ${
                      clipFilter === 'all' ? 'bg-blue-600 text-white' : 'text-slate-400 hover:text-slate-200'
                    }`}
                  >
                    All ({clips.length})
                  </button>
                  <button
                    onClick={() => setClipFilter('pinned')}
                    className={`px-2.5 py-1 text-xs rounded font-medium transition flex items-center gap-1 ${
                      clipFilter === 'pinned' ? 'bg-amber-600 text-white' : 'text-slate-400 hover:text-slate-200'
                    }`}
                  >
                    <Pin className="w-3 h-3" />
                    <span>Pinned ({clips.filter((c) => c.pinned).length})</span>
                  </button>
                  <button
                    onClick={() => setClipFilter('text')}
                    className={`px-2.5 py-1 text-xs rounded font-medium transition ${
                      clipFilter === 'text' ? 'bg-blue-600 text-white' : 'text-slate-400 hover:text-slate-200'
                    }`}
                  >
                    Text
                  </button>
                  <button
                    onClick={() => setClipFilter('image')}
                    className={`px-2.5 py-1 text-xs rounded font-medium transition ${
                      clipFilter === 'image' ? 'bg-blue-600 text-white' : 'text-slate-400 hover:text-slate-200'
                    }`}
                  >
                    Images
                  </button>
                </div>

                <button
                  onClick={() => setShowClearConfirm(true)}
                  className="px-3 py-1.5 text-xs font-medium bg-red-950/40 hover:bg-red-900/60 text-red-300 border border-red-800/60 rounded-lg transition flex items-center gap-1.5"
                  title="Clear non-pinned clips"
                >
                  <Trash2 className="w-3.5 h-3.5 text-red-400" />
                  <span className="hidden sm:inline">Clear History</span>
                </button>
              </div>
            </div>

            {/* List of Clips (GtkListBox implementation) */}
            <div className="bg-slate-950 rounded-xl border border-slate-800 shadow-sm overflow-hidden flex flex-col">
              <div className="px-4 py-3 bg-slate-900/50 border-b border-slate-800 flex items-center justify-between text-xs text-slate-400">
                <div className="flex items-center gap-2">
                  <span className="font-semibold text-slate-300">Clipboard History Log</span>
                  <span>({filteredClips.length} items)</span>
                </div>
                <div className="flex items-center gap-4">
                  <span>SQLite WAL Engine</span>
                </div>
              </div>

              <div className="divide-y divide-slate-800/60 max-h-[500px] overflow-y-auto">
                {filteredClips.length === 0 ? (
                  <div className="py-12 text-center text-slate-400 text-sm">
                    No clipboard items found matching the current search.
                  </div>
                ) : (
                  filteredClips.map((clip) => (
                    <div
                      key={clip.id}
                      className={`p-4 hover:bg-slate-900/50 transition flex items-start justify-between gap-4 group ${
                        clip.pinned ? 'bg-amber-950/10' : ''
                      }`}
                    >
                      {/* Left: Icon / Thumbnail */}
                      <div className="flex items-start gap-3.5 min-w-0 flex-1">
                        {clip.type === 'image' ? (
                          <div className="w-16 h-16 rounded-lg bg-slate-800 border border-slate-700 overflow-hidden shrink-0 flex items-center justify-center relative shadow-sm">
                            <img
                              src={clip.content}
                              alt="Thumbnail"
                              className="w-full h-full object-cover"
                              onError={(e) => {
                                (e.target as HTMLElement).style.display = 'none';
                              }}
                            />
                            <span className="absolute bottom-0 inset-x-0 bg-slate-950/80 text-[9px] text-center text-slate-300 py-0.5">
                              {clip.dimensions || 'PNG'}
                            </span>
                          </div>
                        ) : (
                          <div className="w-10 h-10 rounded-lg bg-slate-800 border border-slate-700 flex items-center justify-center shrink-0 shadow-sm mt-0.5">
                            <FileText className="w-5 h-5 text-blue-400" />
                          </div>
                        )}

                        {/* Middle: Content & Metadata */}
                        <div className="min-w-0 flex-1">
                          <div className="flex items-center gap-2 flex-wrap mb-1">
                            {clip.pinned && (
                              <span className="text-[10px] font-bold px-2 py-0.5 rounded-full bg-amber-500/20 text-amber-300 border border-amber-500/40 flex items-center gap-1">
                                <Pin className="w-2.5 h-2.5 fill-current" />
                                <span>PINNED</span>
                              </span>
                            )}

                            <span
                              className={`text-[10px] font-bold px-2 py-0.5 rounded-full border ${
                                clip.type === 'text'
                                  ? 'bg-blue-950 text-blue-300 border-blue-800'
                                  : 'bg-emerald-950 text-emerald-300 border-emerald-800'
                              }`}
                            >
                              {clip.type.toUpperCase()}
                            </span>

                            <span className="text-xs text-slate-400 flex items-center gap-1">
                              <Clock className="w-3 h-3 text-slate-500" />
                              <span>{formatTimeAgo(clip.timestamp)}</span>
                            </span>

                            <span className="text-[11px] text-slate-500 font-mono">
                              {clip.type === 'text' ? `${clip.charCount} chars` : clip.fileSizeStr}
                            </span>
                          </div>

                          {/* Preview Text */}
                          <div className="font-mono text-xs text-slate-200 bg-slate-900/60 p-2 rounded-md border border-slate-800/80 break-all select-all">
                            {clip.preview}
                          </div>

                          <div className="text-[10px] text-slate-500 font-mono mt-1">
                            SHA-256: {clip.hash}
                          </div>
                        </div>
                      </div>

                      {/* Right: Action Buttons */}
                      <div className="flex items-center gap-1.5 shrink-0 self-center">
                        {/* Copy Again Button */}
                        <button
                          onClick={() => handleCopyClipAgain(clip)}
                          className="p-2 text-xs font-medium bg-slate-800 hover:bg-slate-700 text-slate-200 rounded-lg border border-slate-700 transition"
                          title="Copy again to clipboard"
                        >
                          <Copy className="w-3.5 h-3.5 text-blue-400" />
                        </button>

                        {/* Save As Button */}
                        <button
                          onClick={() => handleSaveClipAs(clip)}
                          className="p-2 text-xs font-medium bg-slate-800 hover:bg-slate-700 text-slate-200 rounded-lg border border-slate-700 transition"
                          title="Save clip as file (.txt or .png)"
                        >
                          <Save className="w-3.5 h-3.5 text-emerald-400" />
                        </button>

                        {/* Pin Button */}
                        <button
                          onClick={() => handleTogglePin(clip.id)}
                          className={`p-2 text-xs font-medium rounded-lg border transition ${
                            clip.pinned
                              ? 'bg-amber-950 text-amber-300 border-amber-700'
                              : 'bg-slate-800 hover:bg-slate-700 text-slate-400 hover:text-amber-300 border-slate-700'
                          }`}
                          title={clip.pinned ? 'Unpin clip' : 'Pin clip to top'}
                        >
                          <Pin className={`w-3.5 h-3.5 ${clip.pinned ? 'fill-current' : ''}`} />
                        </button>

                        {/* Delete Button */}
                        <button
                          onClick={() => handleDeleteClip(clip.id)}
                          className="p-2 text-xs font-medium bg-slate-800 hover:bg-red-900/40 text-slate-400 hover:text-red-300 rounded-lg border border-slate-700 hover:border-red-700 transition"
                          title="Delete this clip"
                        >
                          <Trash2 className="w-3.5 h-3.5" />
                        </button>
                      </div>
                    </div>
                  ))
                )}
              </div>

              {/* Clipboard Status Bar */}
              <div className="px-4 py-2.5 bg-slate-950 border-t border-slate-800 flex items-center justify-between text-xs text-slate-500">
                <div className="flex items-center gap-3">
                  <span>Database: ~/.local/share/appclip-manager/clipboard.db</span>
                  <span>•</span>
                  <span>WAL Mode Active</span>
                </div>
                <span>Created by: Jeyaul Hoque</span>
              </div>
            </div>
          </div>
        )}

        {/* ========================================================================= */}
        {/* TAB 3: APP UPDATE CHECKER (PART 3) */}
        {/* ========================================================================= */}
        {activeTab === 'updates' && (
          <div className="flex flex-col gap-4">
            {/* Updates Header Controls Bar */}
            <div className="bg-slate-950 p-4 rounded-xl border border-slate-800 shadow-sm flex flex-col md:flex-row gap-3 items-center justify-between">
              {/* Left: Summary & Last Checked */}
              <div className="flex items-center gap-3 w-full md:w-auto">
                <div className="w-10 h-10 rounded-xl bg-slate-900 border border-slate-800 flex items-center justify-center text-emerald-400 shrink-0">
                  <ArrowUpCircle className="w-5 h-5" />
                </div>
                <div>
                  <div className="flex items-center gap-2">
                    <span className="text-sm font-bold text-white">System Software Updates</span>
                    <span
                      className={`text-[10px] font-bold px-2 py-0.5 rounded-full border ${
                        updates.length > 0
                          ? 'bg-emerald-950/80 text-emerald-300 border-emerald-700/80'
                          : 'bg-slate-900 text-slate-400 border-slate-800'
                      }`}
                    >
                      {updates.length > 0 ? `${updates.length} Available` : 'Up to Date'}
                    </span>
                  </div>
                  <div className="text-xs text-slate-400 flex items-center gap-2 mt-0.5">
                    <Clock className="w-3.5 h-3.5 text-slate-500" />
                    <span>Last checked: {formatTimeAgo(lastUpdateCheckTime)}</span>
                    <span>•</span>
                    <span className="text-slate-500">APT (pkexec), Snap, Flatpak</span>
                  </div>
                </div>
              </div>

              {/* Right: Action Buttons */}
              <div className="flex items-center gap-2 w-full md:w-auto justify-end">
                <button
                  onClick={handleCheckUpdates}
                  disabled={isCheckingUpdates || isUpdatingAll}
                  className="px-3.5 py-2 text-xs font-medium bg-slate-900 hover:bg-slate-800 disabled:opacity-50 text-slate-200 border border-slate-700 rounded-lg transition flex items-center gap-2"
                  title="Query APT repositories (via pkexec), Snap store, and Flatpak remotes"
                >
                  <RefreshCw className={`w-3.5 h-3.5 text-blue-400 ${isCheckingUpdates ? 'animate-spin' : ''}`} />
                  <span>{isCheckingUpdates ? 'Checking repositories...' : 'Check for Updates'}</span>
                </button>

                <button
                  onClick={handleUpdateAll}
                  disabled={updates.length === 0 || isUpdatingAll || isCheckingUpdates}
                  className="px-4 py-2 text-xs font-semibold bg-emerald-600 hover:bg-emerald-500 disabled:opacity-40 text-white rounded-lg transition shadow-sm flex items-center gap-2"
                  title="Sequentially install all pending updates across APT, Snap, and Flatpak"
                >
                  <Sparkles className="w-3.5 h-3.5" />
                  <span>Update All {updates.length > 0 ? `(${updates.length})` : ''}</span>
                </button>
              </div>
            </div>

            {/* Sequential Batch Update Progress Banner */}
            {isUpdatingAll && (
              <div className="bg-slate-950 border border-emerald-800/60 p-4 rounded-xl shadow-lg flex flex-col gap-2.5 animate-pulse">
                <div className="flex items-center justify-between text-xs">
                  <div className="flex items-center gap-2 text-emerald-400 font-medium">
                    <RefreshCw className="w-3.5 h-3.5 animate-spin" />
                    <span>Updating {updateAllProgress.current} of {updateAllProgress.total}: {updateAllProgress.currentPackage}</span>
                  </div>
                  <span className="text-slate-400 font-mono text-[11px]">
                    {Math.round((updateAllProgress.current / updateAllProgress.total) * 100)}%
                  </span>
                </div>
                <div className="w-full bg-slate-900 rounded-full h-2 overflow-hidden border border-slate-800">
                  <div
                    className="bg-emerald-500 h-2 rounded-full transition-all duration-500"
                    style={{
                      width: `${(updateAllProgress.current / updateAllProgress.total) * 100}%`
                    }}
                  />
                </div>
              </div>
            )}

            {/* List of Updatable Packages (GTK3 GtkListBox representation) */}
            <div className="bg-slate-950 rounded-xl border border-slate-800 shadow-sm overflow-hidden flex flex-col">
              <div className="px-4 py-3 bg-slate-900/50 border-b border-slate-800 flex items-center justify-between text-xs text-slate-400">
                <div className="flex items-center gap-2">
                  <span className="font-semibold text-slate-300">Pending Application Updates</span>
                  <span>({updates.length} items)</span>
                </div>
                <div className="flex items-center gap-6">
                  <span className="hidden sm:inline">Version Transition</span>
                  <span className="hidden md:inline">Download</span>
                  <span>Action</span>
                </div>
              </div>

              {updates.length === 0 ? (
                /* Empty State: System up to date */
                <div className="py-16 px-4 flex flex-col items-center justify-center text-center gap-4">
                  <div className="w-16 h-16 rounded-2xl bg-emerald-950/40 border border-emerald-800/60 flex items-center justify-center text-emerald-400 shadow-inner">
                    <CheckCircle2 className="w-9 h-9" />
                  </div>
                  <div className="flex flex-col gap-1 max-w-md">
                    <h3 className="text-base font-bold text-white">Your system is up to date!</h3>
                    <p className="text-xs text-slate-400 leading-relaxed">
                      All monitored applications and packages from APT, Snap, and Flatpak are running the latest versions.
                    </p>
                    <span className="text-[11px] text-slate-500 font-mono mt-1">
                      Last checked {formatTimeAgo(lastUpdateCheckTime)}
                    </span>
                  </div>
                  <button
                    onClick={handleCheckUpdates}
                    disabled={isCheckingUpdates}
                    className="mt-2 px-4 py-2 text-xs font-medium bg-slate-900 hover:bg-slate-800 text-slate-200 border border-slate-700 rounded-lg transition flex items-center gap-2"
                  >
                    <RefreshCw className={`w-3.5 h-3.5 text-blue-400 ${isCheckingUpdates ? 'animate-spin' : ''}`} />
                    <span>Check Again</span>
                  </button>
                </div>
              ) : (
                <div className="divide-y divide-slate-800/60 max-h-[500px] overflow-y-auto">
                  {updates.map((item) => (
                    <div
                      key={item.id}
                      className="p-4 hover:bg-slate-900/40 transition flex items-center justify-between gap-4 group"
                    >
                      {/* Left: Icon and Details */}
                      <div className="flex items-center gap-3.5 min-w-0 flex-1">
                        <div className="w-11 h-11 rounded-xl bg-slate-900 border border-slate-800 flex items-center justify-center shrink-0 shadow-sm">
                          {item.type === 'APT' && <Package className="w-5 h-5 text-blue-400" />}
                          {item.type === 'Snap' && <Package className="w-5 h-5 text-orange-400" />}
                          {item.type === 'Flatpak' && <Package className="w-5 h-5 text-emerald-400" />}
                        </div>

                        <div className="min-w-0 flex-1">
                          <div className="flex items-center gap-2 flex-wrap mb-1">
                            <span className="text-sm font-bold text-white group-hover:text-blue-400 transition">
                              {item.displayName}
                            </span>

                            {/* Packaging Source Badge */}
                            <span
                              className={`text-[10px] font-bold px-2 py-0.5 rounded-full border ${
                                item.type === 'APT'
                                  ? 'bg-blue-950 text-blue-300 border-blue-800'
                                  : item.type === 'Snap'
                                  ? 'bg-orange-950 text-orange-300 border-orange-800'
                                  : 'bg-emerald-950 text-emerald-300 border-emerald-800'
                              }`}
                            >
                              {item.type}
                            </span>

                            <span className="text-[11px] text-slate-500 font-mono">
                              ({item.packageName})
                            </span>
                          </div>

                          <div className="flex items-center gap-2 text-xs text-slate-400">
                            <span className="hidden sm:inline text-slate-400">{item.description}</span>
                          </div>
                        </div>
                      </div>

                      {/* Middle: Version diff & Download size */}
                      <div className="flex items-center gap-4 shrink-0">
                        <div className="flex items-center gap-1.5 text-xs bg-slate-900/80 px-2.5 py-1 rounded-lg border border-slate-800 font-mono">
                          <span className="text-slate-400">{item.currentVersion}</span>
                          <ArrowRight className="w-3.5 h-3.5 text-slate-500" />
                          <span className="text-emerald-400 font-bold">{item.newVersion}</span>
                        </div>

                        <span className="text-xs text-slate-400 font-mono hidden md:inline w-18 text-right">
                          {item.downloadSize}
                        </span>

                        {/* Right: Individual Update Button */}
                        <button
                          onClick={() => handleSingleUpdate(item)}
                          disabled={updatingRowId === item.id || isUpdatingAll}
                          className="px-3 py-1.5 text-xs font-semibold bg-blue-600 hover:bg-blue-500 disabled:opacity-50 text-white rounded-lg transition flex items-center gap-1.5 shadow-sm"
                          title={`Update ${item.displayName} to version ${item.newVersion}`}
                        >
                          {updatingRowId === item.id ? (
                            <>
                              <RefreshCw className="w-3 h-3 animate-spin" />
                              <span>Updating...</span>
                            </>
                          ) : (
                            <>
                              <ArrowUpCircle className="w-3.5 h-3.5" />
                              <span>Update</span>
                            </>
                          )}
                        </button>
                      </div>
                    </div>
                  ))}
                </div>
              )}

              {/* Statusbar */}
              <div className="px-4 py-2.5 bg-slate-950 border-t border-slate-800 flex items-center justify-between text-xs text-slate-500">
                <div className="flex items-center gap-3">
                  <span>Backend: update_checker.c (fork() + execvp())</span>
                  <span>•</span>
                  <span>Thread Pool: g_thread_new() + g_idle_add()</span>
                </div>
                <span>Auto-check interval: 4 hours (configurable in settings)</span>
              </div>
            </div>
          </div>
        )}

        {/* ========================================================================= */}
        {/* TAB 3: C SOURCE CODE & ARCHITECTURE EXPLORER */}
        {/* ========================================================================= */}
        {activeTab === 'source' && (
          <div className="grid grid-cols-1 lg:grid-cols-12 gap-4">
            {/* Left Sidebar: File Tree */}
            <div className="lg:col-span-4 bg-slate-950 rounded-xl border border-slate-800 p-4 flex flex-col gap-3 shadow-sm">
              <div className="flex items-center justify-between">
                <span className="text-xs font-bold text-slate-200">C Project Source Files</span>
                <span className="text-[10px] px-2 py-0.5 rounded bg-slate-800 text-slate-400 font-mono">
                  {sourceFiles.length} files
                </span>
              </div>

              <div className="flex flex-col gap-1 max-h-[560px] overflow-y-auto pr-1">
                {sourceFiles.map((f) => (
                  <button
                    key={f.id}
                    onClick={() => setSelectedFile(f)}
                    className={`text-left p-2.5 rounded-lg text-xs transition flex flex-col gap-0.5 ${
                      selectedFile.id === f.id
                        ? 'bg-blue-600 text-white font-semibold shadow-sm'
                        : 'bg-slate-900/60 hover:bg-slate-900 text-slate-300 border border-slate-800'
                    }`}
                  >
                    <div className="flex items-center justify-between">
                      <span className="font-mono">{f.name}</span>
                      <span
                        className={`text-[9px] px-1.5 py-0.5 rounded font-mono uppercase ${
                          selectedFile.id === f.id
                            ? 'bg-blue-700 text-blue-100'
                            : 'bg-slate-800 text-slate-400'
                        }`}
                      >
                        {f.language}
                      </span>
                    </div>
                    <span
                      className={`text-[11px] truncate ${
                        selectedFile.id === f.id ? 'text-blue-100' : 'text-slate-400'
                      }`}
                    >
                      {f.description}
                    </span>
                  </button>
                ))}
              </div>

              <a
                href="/appclip-manager-v2.0.tar.gz"
                download
                className="mt-2 w-full py-2 bg-blue-600 hover:bg-blue-500 text-white rounded-lg text-xs font-medium flex items-center justify-center gap-2 transition shadow-sm"
              >
                <Download className="w-3.5 h-3.5" />
                <span>Download Source Archive (tar.gz)</span>
              </a>
            </div>

            {/* Right: Code Viewer */}
            <div className="lg:col-span-8 bg-slate-950 rounded-xl border border-slate-800 overflow-hidden flex flex-col shadow-sm">
              <div className="px-4 py-3 bg-slate-900/80 border-b border-slate-800 flex items-center justify-between">
                <div>
                  <span className="font-mono text-xs text-white font-bold">{selectedFile.path}</span>
                  <p className="text-[11px] text-slate-400">{selectedFile.description}</p>
                </div>

                <button
                  onClick={copySourceCode}
                  className="px-3 py-1 text-xs bg-slate-800 hover:bg-slate-700 text-slate-300 rounded-md border border-slate-700 transition flex items-center gap-1.5"
                >
                  {copiedCode ? <Check className="w-3.5 h-3.5 text-emerald-400" /> : <Copy className="w-3.5 h-3.5" />}
                  <span>{copiedCode ? 'Copied' : 'Copy Code'}</span>
                </button>
              </div>

              <pre className="p-4 text-xs font-mono text-slate-200 overflow-auto max-h-[540px] bg-slate-950 leading-relaxed select-all">
                <code>{selectedFile.content}</code>
              </pre>
            </div>
          </div>
        )}

        {/* ========================================================================= */}
        {/* TAB 4: APP ICON & VISUAL BRANDING */}
        {/* ========================================================================= */}
        {activeTab === 'branding' && <IconShowcase />}
      </main>

      {/* Floating Success Toast Revealer */}
      {toastMessage && (
        <div className="fixed bottom-6 left-1/2 -translate-x-1/2 z-50 bg-slate-950 text-slate-100 px-5 py-2.5 rounded-full border border-slate-700 shadow-2xl flex items-center gap-2 text-xs font-medium animate-bounce">
          <CheckCircle2 className="w-4 h-4 text-emerald-400" />
          <span>{toastMessage}</span>
        </div>
      )}

      {/* Uninstall Confirmation Modal (Simulates GTK3 Dialog) */}
      {selectedAppToUninstall && (
        <div className="fixed inset-0 z-50 bg-black/70 backdrop-blur-xs flex items-center justify-center p-4">
          <div className="bg-slate-950 border border-slate-800 rounded-xl max-w-md w-full p-6 shadow-2xl flex flex-col gap-4">
            <div className="flex items-start gap-4">
              <div className="w-10 h-10 rounded-full bg-red-950/60 border border-red-800/80 flex items-center justify-center shrink-0">
                <AlertTriangle className="w-5 h-5 text-red-400" />
              </div>
              <div className="min-w-0 flex-1">
                <h3 className="text-sm font-bold text-white">
                  Uninstall {selectedAppToUninstall.name}?
                </h3>
                <p className="text-xs text-slate-400 mt-1">
                  Are you sure you want to remove <strong>{selectedAppToUninstall.name}</strong> (package:{' '}
                  <code className="bg-slate-900 px-1 py-0.5 rounded text-red-300 font-mono">
                    {selectedAppToUninstall.pkgId}
                  </code>
                  )?
                </p>

                <div className="mt-3 p-2.5 rounded bg-slate-900 border border-slate-800 text-[11px] font-mono text-slate-400">
                  Command: pkexec apt-get remove -y {selectedAppToUninstall.pkgId}
                </div>
              </div>
            </div>

            {isUninstalling && (
              <div className="flex flex-col gap-1.5">
                <div className="w-full bg-slate-900 rounded-full h-2 overflow-hidden">
                  <div
                    className="bg-red-600 h-2 transition-all duration-300 rounded-full"
                    style={{ width: `${uninstallProgress}%` }}
                  ></div>
                </div>
                <span className="text-[10px] text-slate-400 text-center">
                  Executing privilege escalation with PolicyKit (pkexec)...
                </span>
              </div>
            )}

            <div className="flex items-center justify-end gap-2 pt-2 border-t border-slate-800">
              <button
                disabled={isUninstalling}
                onClick={() => setSelectedAppToUninstall(null)}
                className="px-4 py-2 text-xs font-medium bg-slate-800 hover:bg-slate-700 text-slate-300 rounded-lg transition disabled:opacity-50"
              >
                Cancel
              </button>
              <button
                disabled={isUninstalling}
                onClick={handlePerformUninstall}
                className="px-4 py-2 text-xs font-semibold bg-red-600 hover:bg-red-500 text-white rounded-lg transition disabled:opacity-50 flex items-center gap-1.5 shadow-sm"
              >
                <Trash2 className="w-3.5 h-3.5" />
                <span>{isUninstalling ? 'Removing...' : 'Confirm Uninstall'}</span>
              </button>
            </div>
          </div>
        </div>
      )}

      {/* Clear Clipboard History Confirmation Modal */}
      {showClearConfirm && (
        <div className="fixed inset-0 z-50 bg-black/70 backdrop-blur-xs flex items-center justify-center p-4">
          <div className="bg-slate-950 border border-slate-800 rounded-xl max-w-md w-full p-6 shadow-2xl flex flex-col gap-4">
            <div className="flex items-start gap-4">
              <div className="w-10 h-10 rounded-full bg-red-950/60 border border-red-800/80 flex items-center justify-center shrink-0">
                <Trash2 className="w-5 h-5 text-red-400" />
              </div>
              <div className="min-w-0 flex-1">
                <h3 className="text-sm font-bold text-white">Clear Clipboard History?</h3>
                <p className="text-xs text-slate-400 mt-1">
                  This will remove all non-pinned clips and un-link their thumbnail image files from disk.
                  Pinned clips will be safely retained.
                </p>
              </div>
            </div>

            <div className="flex items-center justify-end gap-2 pt-2 border-t border-slate-800">
              <button
                onClick={() => setShowClearConfirm(false)}
                className="px-4 py-2 text-xs font-medium bg-slate-800 hover:bg-slate-700 text-slate-300 rounded-lg transition"
              >
                Cancel
              </button>
              <button
                onClick={handleClearHistory}
                className="px-4 py-2 text-xs font-semibold bg-red-600 hover:bg-red-500 text-white rounded-lg transition shadow-sm"
              >
                Clear Non-Pinned Clips
              </button>
            </div>
          </div>
        </div>
      )}

      {/* GTK Settings Dialog Modal */}
      {showSettings && (
        <div className="fixed inset-0 z-50 bg-black/70 backdrop-blur-xs flex items-center justify-center p-4">
          <div className="bg-slate-950 border border-slate-800 rounded-2xl max-w-lg w-full p-6 shadow-2xl flex flex-col gap-5 relative">
            <button
              onClick={() => setShowSettings(false)}
              className="absolute right-4 top-4 text-slate-400 hover:text-white"
            >
              <X className="w-4 h-4" />
            </button>

            <div className="flex items-center gap-3">
              <div className="w-9 h-9 rounded-xl bg-blue-600/20 border border-blue-500/30 flex items-center justify-center text-blue-400">
                <SettingsIcon className="w-5 h-5" />
              </div>
              <div>
                <h2 className="text-base font-bold text-white">Settings — AppClip Manager</h2>
                <p className="text-xs text-slate-400">Manage system startup behavior and clipboard storage limits</p>
              </div>
            </div>

            {/* Section 1: System Integration (Start on Boot) */}
            <div className="bg-slate-900/90 border border-slate-800 rounded-xl p-4 flex flex-col gap-3">
              <div className="flex items-center justify-between">
                <div className="flex flex-col gap-0.5">
                  <div className="flex items-center gap-2">
                    <Power className="w-4 h-4 text-blue-400" />
                    <span className="text-sm font-semibold text-white">Start on Boot</span>
                    <span className="text-[10px] bg-blue-950 text-blue-300 border border-blue-800 px-1.5 py-0.2 rounded font-mono">
                      XDG Autostart
                    </span>
                  </div>
                  <span className="text-xs text-slate-400">
                    Start AppClip Manager on system startup
                  </span>
                </div>

                {/* Custom Toggle Switch simulating GtkSwitch */}
                <button
                  type="button"
                  onClick={() => {
                    const next = !startOnBoot;
                    setStartOnBoot(next);
                    if (next) {
                      showToast('Autostart enabled: created ~/.config/autostart/appclip-manager.desktop');
                    } else {
                      showToast('Autostart disabled: removed desktop file from ~/.config/autostart/');
                    }
                  }}
                  className={`w-12 h-6 rounded-full transition-colors relative p-0.5 border ${
                    startOnBoot ? 'bg-blue-600 border-blue-500' : 'bg-slate-800 border-slate-700'
                  }`}
                  role="switch"
                  aria-checked={startOnBoot}
                >
                  <span
                    className={`block w-5 h-5 rounded-full bg-white shadow-md transform transition-transform ${
                      startOnBoot ? 'translate-x-6' : 'translate-x-0'
                    }`}
                  />
                </button>
              </div>

              {/* Autostart Preview / Subtitle Details */}
              <div className="text-xs text-slate-400 bg-slate-950/70 border border-slate-800/80 rounded-lg p-3 flex flex-col gap-1.5 font-mono">
                <div className="flex items-center justify-between text-slate-400">
                  <span>File: ~/.config/autostart/appclip-manager.desktop</span>
                  <span className={`text-[10px] px-1.5 py-0.2 rounded font-sans font-medium ${
                    startOnBoot ? 'bg-emerald-950 text-emerald-400 border border-emerald-800' : 'bg-slate-800 text-slate-400'
                  }`}>
                    {startOnBoot ? 'Active' : 'Disabled'}
                  </span>
                </div>
                {startOnBoot && (
                  <div className="text-[11px] text-slate-300 whitespace-pre font-mono bg-slate-900 p-2 rounded border border-slate-800">
{`[Desktop Entry]
Type=Application
Name=AppClip Manager
Comment=Clipboard history monitor and app manager
Exec=/usr/local/bin/appclip-manager --minimized
Icon=appclip-manager
Terminal=false
X-GNOME-Autostart-enabled=true
Hidden=false`}
                  </div>
                )}
              </div>

              <p className="text-[11px] text-slate-500 leading-relaxed">
                Note: without a system tray, minimized mode runs invisibly in the background. Launch AppClip Manager again from the app menu to open the window.
              </p>
            </div>

            {/* Section 2: Clipboard History Retention */}
            <div className="bg-slate-900/90 border border-slate-800 rounded-xl p-4 flex flex-col gap-3">
              <span className="text-sm font-semibold text-white">Clipboard History Retention</span>

              <div className="grid grid-cols-1 sm:grid-cols-2 gap-4">
                <div className="flex flex-col gap-1.5">
                  <label className="text-xs text-slate-300 flex items-center justify-between">
                    <span>Max history items:</span>
                    <span className="font-mono text-blue-400 font-bold">{maxHistoryItems}</span>
                  </label>
                  <input
                    type="number"
                    min={10}
                    max={10000}
                    step={50}
                    value={maxHistoryItems}
                    onChange={(e) => setMaxHistoryItems(Math.max(10, parseInt(e.target.value) || 10))}
                    className="bg-slate-950 border border-slate-800 rounded-lg px-3 py-1.5 text-xs text-white focus:outline-none focus:border-blue-500 font-mono"
                  />
                </div>

                <div className="flex flex-col gap-1.5">
                  <label className="text-xs text-slate-300 flex items-center justify-between">
                    <span>Retention duration:</span>
                    <span className="font-mono text-blue-400 font-bold">{maxHistoryDays} days</span>
                  </label>
                  <input
                    type="number"
                    min={1}
                    max={365}
                    step={1}
                    value={maxHistoryDays}
                    onChange={(e) => setMaxHistoryDays(Math.max(1, parseInt(e.target.value) || 1))}
                    className="bg-slate-950 border border-slate-800 rounded-lg px-3 py-1.5 text-xs text-white focus:outline-none focus:border-blue-500 font-mono"
                  />
                </div>
              </div>

              <p className="text-[11px] text-slate-400">
                Pinned clips are permanently protected from automatic maintenance pruning.
              </p>
            </div>

            {/* Modal Actions */}
            <div className="flex items-center justify-end gap-2 pt-2 border-t border-slate-800">
              <button
                type="button"
                onClick={() => setShowSettings(false)}
                className="px-4 py-2 text-xs font-medium bg-slate-800 hover:bg-slate-700 text-slate-300 rounded-lg transition"
              >
                Cancel
              </button>
              <button
                type="button"
                onClick={() => {
                  setShowSettings(false);
                  showToast('Settings saved to ~/.config/appclip-manager/settings.conf');
                }}
                className="px-4 py-2 text-xs font-semibold bg-blue-600 hover:bg-blue-500 text-white rounded-lg transition shadow-sm"
              >
                Save Settings
              </button>
            </div>
          </div>
        </div>
      )}

      {/* GTK About Dialog Modal */}
      {showAbout && (
        <div className="fixed inset-0 z-50 bg-black/70 backdrop-blur-xs flex items-center justify-center p-4">
          <div className="bg-slate-950 border border-slate-800 rounded-2xl max-w-md w-full p-6 shadow-2xl flex flex-col items-center text-center gap-4 relative">
            <button
              onClick={() => setShowAbout(false)}
              className="absolute right-4 top-4 text-slate-400 hover:text-white"
            >
              <X className="w-4 h-4" />
            </button>

            {/* 128x128 High-Res Logo */}
            <div className="w-24 h-24 rounded-2xl flex items-center justify-center p-1 drop-shadow-xl">
              <img
                src="/icons/appclip-manager-128x128.png"
                alt="AppClip Logo"
                className="w-full h-full object-contain"
                onError={(e) => {
                  (e.target as HTMLImageElement).src = '/logo.png';
                }}
              />
            </div>

            <div>
              <h2 className="text-lg font-bold text-white">AppClip Manager</h2>
              <span className="text-xs text-slate-400 font-mono">Version 2.0.0 (GTK3 & SQLite3)</span>
            </div>

            <p className="text-xs text-slate-300 leading-relaxed max-w-sm">
              A native Linux desktop application featuring three core modules:
              <br />
              <strong>Part 1:</strong> Installed Applications Manager (APT, Snap, Flatpak, Desktop Entries).
              <br />
              <strong>Part 2:</strong> Clipboard History Manager with SQLite3 persistence, image thumbnails, and X11/Wayland support.
              <br />
              <strong>Part 3:</strong> App Update Checker with background detection and sequential batch upgrades.
            </p>

            <div className="w-full p-3 rounded-xl bg-slate-900/80 border border-slate-800 flex flex-col gap-1.5 text-xs">
              <div className="flex justify-between items-center text-slate-400">
                <span>Created by:</span>
                <strong className="text-white font-medium">Jeyaul Hoque</strong>
              </div>
              <div className="flex justify-between items-center text-slate-400">
                <span>Website:</span>
                <a
                  href="https://jeyaulhoque.pages.dev/"
                  target="_blank"
                  rel="noreferrer"
                  className="text-blue-400 hover:text-blue-300 flex items-center gap-1 font-medium underline underline-offset-2"
                >
                  <span>jeyaulhoque.pages.dev</span>
                  <ExternalLink className="w-3 h-3" />
                </a>
              </div>
              <div className="flex justify-between items-center text-slate-400">
                <span>License:</span>
                <span className="text-slate-300 font-mono">GNU GPL v3.0</span>
              </div>
            </div>

            <div className="w-full flex items-center gap-2">
              <button
                onClick={() => {
                  setShowAbout(false);
                  setActiveTab('branding');
                }}
                className="flex-1 py-2 bg-orange-600/20 hover:bg-orange-600/30 text-orange-400 border border-orange-500/30 rounded-lg text-xs font-medium transition flex items-center justify-center gap-1.5"
              >
                <Palette className="w-3.5 h-3.5" />
                <span>View Brand & Icons</span>
              </button>
              <button
                onClick={() => setShowAbout(false)}
                className="flex-1 py-2 bg-slate-800 hover:bg-slate-700 text-slate-200 rounded-lg text-xs font-medium transition"
              >
                Close
              </button>
            </div>
          </div>
        </div>
      )}
    </div>
  );
}

export default App;


