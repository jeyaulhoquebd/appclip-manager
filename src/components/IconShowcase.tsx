import React, { useState } from 'react';
import {
  Download,
  Palette,
  Check,
  Layers,
  Sparkles,
  Eye,
  FileCode,
  Archive,
  Info
} from 'lucide-react';

interface IconSizeSpec {
  size: number;
  label: string;
  useCase: string;
}

const ICON_SIZES: IconSizeSpec[] = [
  { size: 512, label: '512 × 512', useCase: 'Master Asset • App Store / Flathub / HiDPI Displays' },
  { size: 256, label: '256 × 256', useCase: 'GNOME Dash / Application Grid & Retina switchers' },
  { size: 128, label: '128 × 128', useCase: 'GtkAboutDialog & GNOME Software details view' },
  { size: 64, label: '64 × 64', useCase: 'Notification banners & Application launcher popups' },
  { size: 48, label: '48 × 48', useCase: 'Standard Ubuntu Yaru desktop & panel menu icon' },
  { size: 32, label: '32 × 32', useCase: 'GtkHeaderBar app identity & Window list icons' },
  { size: 16, label: '16 × 16', useCase: 'Window titlebar corner, mini taskbar & favicons' }
];

export function IconShowcase() {
  const [palette, setPalette] = useState<'ubuntu' | 'cool'>('ubuntu');
  const [bgStyle, setBgStyle] = useState<'aubergine' | 'dark' | 'light' | 'checker'>('aubergine');
  const [activeTab, setActiveTab] = useState<'preview' | 'spec' | 'integration'>('preview');

  const getIconPath = (size: number) => {
    return palette === 'ubuntu'
      ? `/icons/ubuntu/appclip-manager-${size}x${size}.png`
      : `/icons/cool/appclip-manager-${size}x${size}.png`;
  };

  const getSvgPath = () => {
    return palette === 'ubuntu'
      ? '/icons/appclip-manager-ubuntu.svg'
      : '/icons/appclip-manager-cool.svg';
  };

  const getBackgroundClass = () => {
    switch (bgStyle) {
      case 'aubergine':
        return 'bg-gradient-to-br from-[#2C001E] via-[#4A1538] to-[#1a0012]';
      case 'dark':
        return 'bg-slate-900 border border-slate-800';
      case 'light':
        return 'bg-slate-100 border border-slate-300';
      case 'checker':
        return 'bg-[linear-gradient(45deg,#1e293b_25%,transparent_25%),linear-gradient(-45deg,#1e293b_25%,transparent_25%),linear-gradient(45deg,transparent_75%,#1e293b_75%),linear-gradient(-45deg,transparent_75%,#1e293b_75%)] bg-[size:16px_16px] bg-[position:0_0,0_8px,8px_-8px,-8px_0px] bg-slate-950 border border-slate-800';
    }
  };

  return (
    <div className="flex-1 overflow-y-auto p-4 md:p-6 max-w-7xl mx-auto w-full space-y-6">
      {/* Top Banner */}
      <div className="bg-slate-950 border border-slate-800 rounded-xl p-5 shadow-sm flex flex-col md:flex-row md:items-center justify-between gap-4">
        <div>
          <div className="flex items-center gap-2 mb-1.5">
            <span className="p-1.5 bg-orange-500/10 text-orange-400 rounded-lg border border-orange-500/20">
              <Palette className="w-4 h-4" />
            </span>
            <h2 className="text-lg font-bold text-white tracking-tight">AppClip Manager — Icon & Brand Identity</h2>
          </div>
          <p className="text-sm text-slate-400 max-w-2xl">
            Modern, minimalist Ubuntu/GNOME squircle icon uniting the <span className="text-orange-400 font-medium">Installed Apps Grid</span> and the <span className="text-blue-400 font-medium">Clipboard History Board</span>. Scales cleanly from 512×512 down to 16×16.
          </p>
        </div>

        {/* Global Download Pack */}
        <div className="flex items-center gap-3">
          <a
            href="/appclip-manager-icons.tar.gz"
            download
            className="px-4 py-2 bg-gradient-to-r from-orange-600 to-amber-600 hover:from-orange-500 hover:to-amber-500 text-white font-medium text-xs rounded-lg shadow-sm transition flex items-center gap-2"
          >
            <Archive className="w-4 h-4" />
            <span>Download All Icons (.tar.gz)</span>
          </a>
          <a
            href={getSvgPath()}
            download="appclip-manager.svg"
            className="px-3.5 py-2 bg-slate-800 hover:bg-slate-700 text-slate-200 border border-slate-700 text-xs font-medium rounded-lg transition flex items-center gap-1.5"
          >
            <FileCode className="w-4 h-4 text-emerald-400" />
            <span>Vector SVG</span>
          </a>
        </div>
      </div>

      {/* Control Bar: Theme Palette & Background Switcher */}
      <div className="bg-slate-950 border border-slate-800 rounded-xl p-4 flex flex-wrap items-center justify-between gap-4">
        {/* Color Palette Switcher */}
        <div className="flex items-center gap-2">
          <span className="text-xs font-medium text-slate-400 mr-1">Color Palette:</span>
          <button
            onClick={() => setPalette('ubuntu')}
            className={`px-3 py-1.5 rounded-lg text-xs font-medium flex items-center gap-2 transition border ${
              palette === 'ubuntu'
                ? 'bg-orange-500/15 text-orange-400 border-orange-500/40 shadow-sm'
                : 'bg-slate-900 text-slate-400 border-slate-800 hover:text-slate-200'
            }`}
          >
            <span className="w-3 h-3 rounded-full bg-gradient-to-br from-[#E95420] to-[#2C001E]"></span>
            <span>Ubuntu Warm (#E95420 & #2C001E)</span>
            {palette === 'ubuntu' && <Check className="w-3 h-3 text-orange-400" />}
          </button>

          <button
            onClick={() => setPalette('cool')}
            className={`px-3 py-1.5 rounded-lg text-xs font-medium flex items-center gap-2 transition border ${
              palette === 'cool'
                ? 'bg-purple-500/15 text-purple-400 border-purple-500/40 shadow-sm'
                : 'bg-slate-900 text-slate-400 border-slate-800 hover:text-slate-200'
            }`}
          >
            <span className="w-3 h-3 rounded-full bg-gradient-to-br from-[#7F5AF0] to-[#2CB67D]"></span>
            <span>Cool Alternative (#7F5AF0 & #2CB67D)</span>
            {palette === 'cool' && <Check className="w-3 h-3 text-purple-400" />}
          </button>
        </div>

        {/* Background Simulator */}
        <div className="flex items-center gap-2">
          <span className="text-xs font-medium text-slate-400 mr-1">Preview Backdrop:</span>
          <div className="inline-flex rounded-lg bg-slate-900 p-0.5 border border-slate-800">
            <button
              onClick={() => setBgStyle('aubergine')}
              className={`px-2.5 py-1 text-xs rounded-md font-medium transition ${
                bgStyle === 'aubergine' ? 'bg-[#2C001E] text-white' : 'text-slate-400 hover:text-slate-200'
              }`}
            >
              Ubuntu Yaru
            </button>
            <button
              onClick={() => setBgStyle('dark')}
              className={`px-2.5 py-1 text-xs rounded-md font-medium transition ${
                bgStyle === 'dark' ? 'bg-slate-800 text-white' : 'text-slate-400 hover:text-slate-200'
              }`}
            >
              Dark Slate
            </button>
            <button
              onClick={() => setBgStyle('light')}
              className={`px-2.5 py-1 text-xs rounded-md font-medium transition ${
                bgStyle === 'light' ? 'bg-white text-slate-900' : 'text-slate-400 hover:text-slate-200'
              }`}
            >
              Light Desk
            </button>
            <button
              onClick={() => setBgStyle('checker')}
              className={`px-2.5 py-1 text-xs rounded-md font-medium transition ${
                bgStyle === 'checker' ? 'bg-slate-700 text-white' : 'text-slate-400 hover:text-slate-200'
              }`}
            >
              Transparency
            </button>
          </div>
        </div>
      </div>

      {/* Main Showcase Grid */}
      <div className="grid grid-cols-1 lg:grid-cols-12 gap-6">
        {/* Left Column: 512x512 Master Showcase */}
        <div className="lg:col-span-6 bg-slate-950 border border-slate-800 rounded-xl p-6 flex flex-col items-center justify-between">
          <div className="w-full flex items-center justify-between mb-4">
            <div className="flex items-center gap-2">
              <span className="w-2 h-2 rounded-full bg-emerald-400"></span>
              <span className="text-xs font-semibold text-slate-300 uppercase tracking-wider">512 × 512 Master Asset</span>
            </div>
            <span className="text-xs text-slate-500 font-mono">RGBA • Transparent PNG</span>
          </div>

          {/* Master Icon Canvas Container */}
          <div className={`w-full max-w-[380px] aspect-square rounded-2xl flex items-center justify-center p-8 transition-all duration-300 ${getBackgroundClass()}`}>
            <img
              src={getIconPath(512)}
              alt="AppClip Manager 512x512 Master Icon"
              className="w-full h-full object-contain filter drop-shadow-2xl hover:scale-105 transition-transform duration-300 select-none"
            />
          </div>

          {/* Master Details & Download */}
          <div className="w-full mt-6 pt-4 border-t border-slate-800/80 flex items-center justify-between">
            <div>
              <div className="text-xs font-medium text-slate-200">
                {palette === 'ubuntu' ? 'Ubuntu Classic Palette' : 'Cool Modern Gradient'}
              </div>
              <div className="text-[11px] text-slate-500">
                {palette === 'ubuntu' ? '#E95420 (Orange) → #2C001E (Aubergine)' : '#7F5AF0 (Violet) → #2CB67D (Emerald)'}
              </div>
            </div>

            <a
              href={getIconPath(512)}
              download={`appclip-manager-512x512-${palette}.png`}
              className="px-3 py-1.5 bg-slate-800 hover:bg-slate-700 text-slate-200 border border-slate-700 rounded-lg text-xs font-medium transition flex items-center gap-1.5"
            >
              <Download className="w-3.5 h-3.5 text-blue-400" />
              <span>Download 512px</span>
            </a>
          </div>
        </div>

        {/* Right Column: Scalability Matrix (All Requested Sizes) */}
        <div className="lg:col-span-6 bg-slate-950 border border-slate-800 rounded-xl p-6 flex flex-col justify-between">
          <div>
            <div className="flex items-center justify-between mb-4">
              <div className="flex items-center gap-2">
                <Layers className="w-4 h-4 text-orange-400" />
                <h3 className="text-sm font-bold text-white tracking-tight">Multi-Resolution Scalability Test</h3>
              </div>
              <span className="text-xs text-slate-400">Tested 16px to 256px</span>
            </div>
            <p className="text-xs text-slate-400 mb-5">
              GNOME HIG requires crisp readability across tiny 16px window icons and dense 48px desktop grids without blur or optical clutter.
            </p>

            {/* List of Resized Icons */}
            <div className="space-y-3">
              {ICON_SIZES.slice(1).map((spec) => (
                <div
                  key={spec.size}
                  className="bg-slate-900/80 border border-slate-800/80 hover:border-slate-700 rounded-xl p-3 flex items-center justify-between transition-all"
                >
                  <div className="flex items-center gap-4">
                    {/* Fixed visual box for icon display */}
                    <div className={`w-14 h-14 rounded-lg flex items-center justify-center p-1.5 ${getBackgroundClass()}`}>
                      <img
                        src={getIconPath(spec.size)}
                        alt={`${spec.size}x${spec.size} icon`}
                        style={{ width: `${Math.min(spec.size, 48)}px`, height: `${Math.min(spec.size, 48)}px` }}
                        className="object-contain"
                      />
                    </div>

                    <div>
                      <div className="flex items-center gap-2">
                        <span className="text-xs font-bold text-white font-mono">{spec.label}</span>
                        <span className="text-[10px] px-1.5 py-0.5 rounded bg-slate-800 text-slate-400 border border-slate-700 font-mono">
                          {spec.size}px
                        </span>
                      </div>
                      <div className="text-[11px] text-slate-400 mt-0.5">
                        {spec.useCase}
                      </div>
                    </div>
                  </div>

                  <a
                    href={getIconPath(spec.size)}
                    download={`appclip-manager-${spec.size}x${spec.size}.png`}
                    className="p-2 text-slate-400 hover:text-white hover:bg-slate-800 rounded-lg transition"
                    title={`Download ${spec.size}x${spec.size} PNG`}
                  >
                    <Download className="w-4 h-4" />
                  </a>
                </div>
              ))}
            </div>
          </div>

          <div className="mt-6 pt-4 border-t border-slate-800 text-xs text-slate-500 flex items-center justify-between">
            <span>Anti-aliased Lanczos filtering with 0.23 GNOME squircle ratio</span>
            <span className="text-emerald-400 font-mono">100% FreeDesktop Compliant</span>
          </div>
        </div>
      </div>

      {/* Linux System Deployment Hierarchy Spec */}
      <div className="bg-slate-950 border border-slate-800 rounded-xl p-5">
        <div className="flex items-center gap-2 mb-3">
          <Info className="w-4 h-4 text-blue-400" />
          <h3 className="text-sm font-bold text-white">FreeDesktop Hicolor Icon Hierarchy & Installation</h3>
        </div>
        <p className="text-xs text-slate-400 mb-4">
          When installing AppClip Manager via <code className="text-blue-400 bg-slate-900 px-1.5 py-0.5 rounded">sudo make install</code> or <code className="text-blue-400 bg-slate-900 px-1.5 py-0.5 rounded">sudo ./install.sh</code>, the icons are automatically provisioned to their respective standard locations:
        </p>

        <div className="grid grid-cols-1 md:grid-cols-2 lg:grid-cols-4 gap-3 text-xs font-mono">
          <div className="p-3 bg-slate-900 rounded-lg border border-slate-800">
            <div className="text-orange-400 font-bold mb-1">/usr/share/icons/hicolor/scalable/apps/</div>
            <div className="text-slate-400 text-[11px]">appclip-manager.svg (Vector)</div>
          </div>
          <div className="p-3 bg-slate-900 rounded-lg border border-slate-800">
            <div className="text-emerald-400 font-bold mb-1">/usr/share/icons/hicolor/512x512/apps/</div>
            <div className="text-slate-400 text-[11px]">appclip-manager.png (512px)</div>
          </div>
          <div className="p-3 bg-slate-900 rounded-lg border border-slate-800">
            <div className="text-blue-400 font-bold mb-1">/usr/share/icons/hicolor/128x128/apps/</div>
            <div className="text-slate-400 text-[11px]">appclip-manager.png (128px & About)</div>
          </div>
          <div className="p-3 bg-slate-900 rounded-lg border border-slate-800">
            <div className="text-purple-400 font-bold mb-1">/usr/share/icons/hicolor/48x48/apps/</div>
            <div className="text-slate-400 text-[11px]">appclip-manager.png (Desktop Grid)</div>
          </div>
        </div>
      </div>
    </div>
  );
}
