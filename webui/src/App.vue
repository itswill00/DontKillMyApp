<template>
  <div class="app-shell">
    <!-- Stable header: always same, quick status + refresh -->
    <header class="page-header">
      <div style="min-width: 0; flex: 1;">
        <div class="page-header-title">DontKillMyApp</div>
        <div class="page-header-sub truncate-text">{{ deviceSub }}</div>
      </div>
      <div style="display: flex; align-items: center; gap: 8px; flex-shrink: 0;">
        <span
          class="badge-pill active"
          style="cursor: pointer; user-select: none;"
          @click="fetchStatus"
          title="Ketuk untuk refresh"
        >
          {{ status.stats.is_pacified ? 'Optimized' : 'Stock' }} · v1.0.0
        </span>
        <button class="btn btn-outline btn-sm" @click="fetchStatus" title="Refresh status" style="padding: 6px 8px;">
          <Icons name="refresh" :size="13" :class="{ 'spin-anim': isRefreshing }" />
        </button>
      </div>
    </header>

    <main class="content-area">
      <!-- ==================== RINGKASAN ==================== -->
      <template v-if="activeTab === 'dashboard'">
        <!-- Card 1: Apa yang modul ini lakukan + tombol utama -->
        <section class="md3-card">
          <div class="card-title-row">
            <span class="card-title">{{ status.stats.is_pacified ? 'Aplikasi dibiarin jalan' : 'Sistem masih batasi background' }}</span>
            <span class="status-indicator" :class="{ active: status.stats.is_pacified }">
              {{ status.stats.is_pacified ? 'Aktif' : 'Bawaan pabrik' }}
            </span>
          </div>
          <p class="card-desc">
            {{ dashboardDesc }}
          </p>
          <!-- What each button does, explicit -->
          <div style="display: flex; flex-direction: column; gap: 6px; margin-top: 2px;">
            <div style="font-size: 10.5px; color: var(--on-surface-variant); line-height: 1.4;">
              <span style="color: var(--on-surface); font-weight: 600;">Aktifkan</span> = matikan semua pengawas yang suka kill app (Phantom, Freezer, Scout, dsb) + bebaskan 392 app.
              <span style="color: var(--outline);"> · </span>
              <span style="color: var(--on-surface); font-weight: 600;">Balikin</span> = kembalikan ke setelan pabrik OEM.
            </div>
          </div>
          <div style="display: flex; gap: 8px; margin-top: 4px;">
            <button class="btn btn-primary" style="flex: 1;" :disabled="isApplying" @click="pacifyAll">
              <span>{{ isApplying ? 'Menerapkan...' : 'Aktifkan biar ga dimatiin' }}</span>
            </button>
            <button class="btn btn-outline" :disabled="isApplying" @click="restoreStock">
              <span>Balikin bawaan</span>
            </button>
          </div>
          <div style="font-size: 10.5px; color: var(--outline); font-variant-numeric: tabular-nums; margin-top: 2px;">
            {{ oemLabel }} · {{ status.stats.available_killers }} pengawas terdeteksi, {{ status.stats.pacified_killers }} udah dijinakin
            <span v-if="status.stats.is_pacified" style="color: var(--on-surface);"> · siap</span>
            <span v-else style="color: var(--outline);"> · belum optimal</span>
          </div>
        </section>

        <!-- Card 2: Memory -->
        <section class="md3-card">
          <div class="card-title-row">
            <span class="card-title">Memory HP</span>
            <span class="card-extra">{{ formatMb(status.ram.used_mb) }} / {{ formatMb(status.ram.total_mb) }} terpakai</span>
          </div>
          <div style="display: flex; flex-direction: column; gap: 10px;">
            <div>
              <div style="display: flex; justify-content: space-between; font-size: 11px; margin-bottom: 4px;">
                <span style="color: var(--on-surface-variant);">RAM fisik</span>
                <span style="font-variant-numeric: tabular-nums;">{{ ramPercent }}% · {{ formatMb(status.ram.free_mb) }} sisa</span>
              </div>
              <div class="bar-track"><div class="bar-fill" :style="{ width: ramPercent + '%' }"></div></div>
            </div>
            <div>
              <div style="display: flex; justify-content: space-between; font-size: 11px; margin-bottom: 4px;">
                <span style="color: var(--on-surface-variant);">ZRAM (swap)</span>
                <span style="font-variant-numeric: tabular-nums;">{{ zramPercent }}% · {{ formatMb(status.ram.zram_used_mb) }} / {{ formatMb(status.ram.zram_total_mb) }}</span>
              </div>
              <div class="bar-track"><div class="bar-fill" :style="{ width: zramPercent + '%', background: 'var(--secondary)' }"></div></div>
            </div>
          </div>
        </section>

        <!-- Card 3: Ringkasan cepat - read-only, tap to jump to Controls -->
        <section class="md3-card">
          <div class="card-title-row">
            <span class="card-title">Ringkasan pengawas di HP ini</span>
            <span class="card-extra" style="cursor: pointer; border-bottom: 1px solid var(--outline); padding-bottom: 1px;" @click="activeTab = 'tunables'">Atur di Mesin →</span>
          </div>
          <p class="card-desc" style="margin-bottom: 2px;">
            Ini cuma ringkasan. Mau nyalain/matiin satu per satu, buka tab <span style="color: var(--on-surface); font-weight: 600;">Mesin</span>.
          </p>
          <div class="status-list">
            <div v-if="status.caps.phantom" class="status-row" style="cursor: pointer;" @click="activeTab = 'tunables'">
              <div class="status-col">
                <div class="status-name">Phantom Process <span style="font-weight: 400; color: var(--on-surface-variant);">— batas proses bayangan</span></div>
                <div class="status-sub">Kalau app buka banyak worker kecil, default 32 bakal dipotong.</div>
              </div>
              <span class="status-badge" :class="{ ok: status.killers.phantom_limit === '2147483647' }">{{ status.killers.phantom_limit === '2147483647' ? 'Dimatikan' : 'Aktif (' + status.killers.phantom_limit + ')' }}</span>
            </div>
            <div v-if="status.caps.freezer" class="status-row" style="cursor: pointer;" @click="activeTab = 'tunables'">
              <div class="status-col">
                <div class="status-name">Cached Freezer <span style="font-weight: 400; color: var(--on-surface-variant);">— pembeku app nganggur</span></div>
                <div class="status-sub">Android 14 bekukan app yang ga dipakai biar hemat.</div>
              </div>
              <span class="status-badge" :class="{ ok: status.killers.cached_freezer === 'false' }">{{ status.killers.cached_freezer === 'false' ? 'Dimatikan' : 'Aktif' }}</span>
            </div>
            <div class="status-row" style="cursor: pointer;" @click="activeTab = 'tunables'">
              <div class="status-col">
                <div class="status-name">LMKD <span style="font-weight: 400; color: var(--on-surface-variant);">— penjaga RAM</span></div>
                <div class="status-sub">Kalau RAM sesak, sistem kill app. Kita longgarkan ambangnya.</div>
              </div>
              <span class="status-badge" :class="{ ok: status.killers.lmkd_psi_partial !== '35' }">{{ status.killers.lmkd_psi_partial }} ms</span>
            </div>
            <div v-if="status.caps.xiaomi" class="status-row" style="cursor: pointer;" @click="activeTab = 'tunables'">
              <div class="status-col">
                <div class="status-name">Xiaomi Scout / SPC / PowerKeeper</div>
                <div class="status-sub">Khusus HyperOS/MIUI — 3 pengawas + 1 freezer bawaan Xiaomi.</div>
              </div>
              <span class="status-badge" :class="{ ok: !status.killers.scout_enable && !status.killers.spc_enable }">{{ (!status.killers.scout_enable && !status.killers.spc_enable) ? 'Dimatikan' : 'Aktif' }}</span>
            </div>
            <div v-if="status.caps.duraspeed" class="status-row" style="cursor: pointer;" @click="activeTab = 'tunables'">
              <div class="status-col">
                <div class="status-name">DuraSpeed <span style="font-weight: 400; color: var(--on-surface-variant);">— MediaTek</span></div>
                <div class="status-sub">Daemon MediaTek yang reklamasi memory di belakang.</div>
              </div>
              <span class="status-badge" :class="{ ok: status.killers.duraspeed_disabled }">{{ status.killers.duraspeed_disabled ? 'Dimatikan' : 'Aktif' }}</span>
            </div>
            <div v-if="status.caps.samsung || status.caps.oplus || status.caps.transsion" class="status-row" style="cursor: pointer;" @click="activeTab = 'tunables'">
              <div class="status-col">
                <div class="status-name">Adaptive Battery <span style="font-weight: 400; color: var(--on-surface-variant);">— {{ status.caps.samsung ? 'OneUI' : status.caps.oplus ? 'ColorOS' : 'XOS' }}</span></div>
                <div class="status-sub">{{ adaptiveSub }}</div>
              </div>
              <span class="status-badge" :class="{ ok: status.stats.is_pacified }">{{ status.stats.is_pacified ? 'Dimatikan' : 'Aktif' }}</span>
            </div>
          </div>
          <div style="font-size: 10.5px; color: var(--outline); margin-top: 4px;">Ketuk baris untuk lompat ke pengaturan detail.</div>
        </section>
      </template>

      <!-- ==================== APLIKASI ==================== -->
      <template v-else-if="activeTab === 'apps'">
        <section class="md3-card">
          <div class="card-title-row">
            <span class="card-title">Per aplikasi</span>
            <span class="card-extra">{{ filteredApps.length }} dari {{ appsList.length }}</span>
          </div>
          <p class="card-desc">
            Di sini atur <span style="color: var(--on-surface); font-weight: 600;">per app</span>, bukan sistem. Kalau WhatsApp/Telegram telat notif, cari app-nya dan set jadi <span style="color: var(--on-surface); font-weight: 600;">Bebas</span>.
            Tombol di bawah cuma untuk app yang kamu lihat (sesuai filter).
          </p>

          <div style="display: flex; gap: 8px;">
            <button class="btn btn-primary btn-sm" style="flex: 1;" @click="setAllUnrestricted">
              Bebasin yang tampil
            </button>
            <button class="btn btn-outline btn-sm" @click="whitelistUserApps">
              Whitelist yang kamu install
            </button>
          </div>

          <div class="search-box">
            <Icons name="search" :size="13" style="color: var(--outline);" />
            <input type="text" v-model="searchQuery" placeholder="Cari app, mis: WhatsApp, Telegram..." />
            <span v-if="searchQuery" @click="searchQuery = ''" style="cursor: pointer; color: var(--outline);"><Icons name="x" :size="12" /></span>
          </div>

          <div class="chips-row">
            <div class="chip" :class="{ active: appFilter === 'all' }" @click="appFilter = 'all'">
              Semua ({{ appsList.length }})
            </div>
            <div class="chip" :class="{ active: appFilter === 'user' }" @click="appFilter = 'user'">
              Kamu install ({{ appsList.filter(a => a.isUser).length }})
            </div>
            <div class="chip" :class="{ active: appFilter === 'restricted' }" @click="appFilter = 'restricted'">
              Dibatasi ({{ appsList.filter(a => a.bgControl !== 'noRestrict').length }})
            </div>
            <div class="chip" :class="{ active: appFilter === 'unrestricted' }" @click="appFilter = 'unrestricted'">
              Bebas ({{ appsList.filter(a => a.bgControl === 'noRestrict').length }})
            </div>
          </div>
          <div style="font-size: 10.5px; color: var(--outline);">Filter ngaruh ke daftar di bawah dan tombol Bebasin.</div>
        </section>

        <section class="md3-card">
          <div v-if="isLoadingApps" style="text-align: center; padding: 20px; color: var(--on-surface-variant);">
            <Icons name="refresh" :size="18" class="spin-anim" />
            <div style="margin-top: 8px; font-size: 12px;">Memuat daftar aplikasi...</div>
          </div>

          <div v-else-if="filteredApps.length === 0" style="text-align: center; padding: 24px; color: var(--on-surface-variant);">
            <div style="font-size: 12.5px;">Ga ada app yang cocok filter</div>
            <div style="font-size: 11px; color: var(--outline); margin-top: 4px;">Coba ganti ke <span style="color: var(--on-surface); font-weight: 600; cursor: pointer; border-bottom: 1px solid var(--outline); padding-bottom: 1px;" @click="appFilter = 'all'; searchQuery = ''">Semua</span> atau hapus pencarian.</div>
          </div>

          <div v-else style="display: flex; flex-direction: column;">
            <div v-for="app in visibleApps" :key="app.pkg" class="list-row" style="cursor: pointer;" @click="toggleAppRestriction(app)">
              <div class="list-row-main">
                <div class="list-row-title">{{ formatAppTitle(app.pkg) }}</div>
                <!-- deslop-ignore-next-line 34 -->
                <div style="font-size: 10.5px; color: var(--outline); font-family: var(--font-mono);">{{ app.pkg }}</div>
                <div style="display: flex; gap: 6px; margin-top: 4px; align-items: center; flex-wrap: wrap;">
                  <span class="status-indicator" :class="{ active: app.bgControl === 'noRestrict' }" style="font-size: 10.5px;">
                    Background: {{ app.bgControl === 'noRestrict' ? 'Bebas' : 'Dibatasi' }}
                  </span>
                  <span style="color: var(--outline);">·</span>
                  <span class="status-indicator" :class="{ active: app.whitelisted }" style="font-size: 10.5px; cursor: pointer;" @click.stop="toggleAppWhitelist(app)">
                    Doze: {{ app.whitelisted ? 'Whitelist' : 'Default' }}
                  </span>
                  <span style="font-size: 10px; color: var(--outline);">— ketuk baris untuk ganti, ketuk Doze untuk whitelist</span>
                </div>
              </div>
              <label class="md3-switch" @click.stop>
                <input type="checkbox" :checked="app.bgControl === 'noRestrict'" @change="toggleAppRestriction(app)" />
                <span class="md3-switch-track"><span class="md3-switch-thumb"></span></span>
              </label>
            </div>
            <div v-if="filteredApps.length > visibleLimit" style="text-align: center; padding: 12px 0 4px 0;">
              <button class="btn btn-outline btn-sm btn-block" @click="visibleLimit += 35">
                Muat lagi — tampil {{ visibleLimit }} dari {{ filteredApps.length }}
              </button>
            </div>
          </div>
        </section>
      </template>

      <!-- ==================== MESIN ==================== -->
      <template v-else-if="activeTab === 'tunables'">
        <section class="md3-card">
          <div class="card-title-row">
            <span class="card-title">Mesin — otak sistem yang matiin app</span>
            <span class="card-extra">{{ status.stats.pacified_killers }}/{{ status.stats.available_killers }} dijinakin</span>
          </div>
          <p class="card-desc">
            Di sini atur <span style="color: var(--on-surface); font-weight: 600;">mesin sistem</span>, bukan per app. Matikan = sistem ga kill. Hidupkan = balik ke bawaan pabrik.
            Yang ga ada di HP ini otomatis hilang.
          </p>

          <div style="display: flex; flex-direction: column; gap: 16px; margin-top: 4px;">
            <!-- Grup: Android universal -->
            <div>
              <div style="font-size: 11px; font-weight: 700; letter-spacing: 0.3px; color: var(--outline); margin-bottom: 6px; display: flex; align-items: center; gap: 6px;">
                <span style="height: 1px; flex: 1; background: var(--outline-variant);"></span>
                ANDROID — semua HP
                <span style="height: 1px; flex: 1; background: var(--outline-variant);"></span>
              </div>
              <div style="display: flex; flex-direction: column;">
                <div v-if="status.caps.phantom" class="list-row">
                  <div class="list-row-main">
                    <div class="list-row-title">Phantom Process</div>
                    <div class="list-row-desc">Batas proses bayangan. Default 32. Matikan = jadi unlimited, worker background ga kepotong. <span style="color: var(--on-surface);">Efek: download / sync di background lebih aman.</span></div>
                  </div>
                  <label class="md3-switch">
                    <input type="checkbox" :checked="status.killers.phantom_limit === '2147483647'" @change="toggleFeature('phantom', status.killers.phantom_limit !== '2147483647')" />
                    <span class="md3-switch-track"><span class="md3-switch-thumb"></span></span>
                  </label>
                </div>
                <div v-if="status.caps.freezer" class="list-row">
                  <div class="list-row-main">
                    <div class="list-row-title">Cached Freezer</div>
                    <div class="list-row-desc">Pembeku app nganggur (cgroup v2, Android 14+). Matikan = app ga dibekukan pas di background. <span style="color: var(--on-surface);">Efek: app balik instan, socket ga ke-freeze.</span></div>
                  </div>
                  <label class="md3-switch">
                    <input type="checkbox" :checked="status.killers.cached_freezer === 'false'" @change="toggleFeature('freezer', status.killers.cached_freezer !== 'false')" />
                    <span class="md3-switch-track"><span class="md3-switch-thumb"></span></span>
                  </label>
                </div>
                <div class="list-row">
                  <div class="list-row-main">
                    <div class="list-row-title">LMKD longgar (250 ms)</div>
                    <div class="list-row-desc">Penjaga RAM. Default 35 ms sensitif, suka kill duluan pas spike. Longgarkan = kill telat dikit, lebih tahan. <span style="color: var(--on-surface);">Efek: multitasking lebih tahan.</span></div>
                  </div>
                  <label class="md3-switch">
                    <input type="checkbox" :checked="status.killers.lmkd_psi_partial !== '35'" @change="toggleFeature('lmkd', status.killers.lmkd_psi_partial === '35')" />
                    <span class="md3-switch-track"><span class="md3-switch-thumb"></span></span>
                  </label>
                </div>
              </div>
            </div>

            <!-- Grup: MediaTek -->
            <div v-if="status.caps.duraspeed">
              <div style="font-size: 11px; font-weight: 700; letter-spacing: 0.3px; color: var(--outline); margin-bottom: 6px; display: flex; align-items: center; gap: 6px;">
                <span style="height: 1px; flex: 1; background: var(--outline-variant);"></span>
                MEDIATEK
                <span style="height: 1px; flex: 1; background: var(--outline-variant);"></span>
              </div>
              <div class="list-row">
                <div class="list-row-main">
                  <div class="list-row-title">DuraSpeed</div>
                  <div class="list-row-desc">Daemon MediaTek yang reklamasi memory diam-diam. Matikan = ga reclaim. <span style="color: var(--on-surface);">Efek: app MTK device ga dimatiin sepihak.</span></div>
                </div>
                <label class="md3-switch">
                  <input type="checkbox" :checked="status.killers.duraspeed_disabled" @change="toggleFeature('duraspeed', !status.killers.duraspeed_disabled)" />
                  <span class="md3-switch-track"><span class="md3-switch-thumb"></span></span>
                </label>
              </div>
            </div>

            <!-- Grup: Xiaomi -->
            <template v-if="status.caps.xiaomi">
              <div>
                <div style="font-size: 11px; font-weight: 700; letter-spacing: 0.3px; color: var(--outline); margin-bottom: 6px; display: flex; align-items: center; gap: 6px;">
                  <span style="height: 1px; flex: 1; background: var(--outline-variant);"></span>
                  XIAOMI — HyperOS / MIUI
                  <span style="height: 1px; flex: 1; background: var(--outline-variant);"></span>
                </div>
                <div style="display: flex; flex-direction: column;">
                  <div class="list-row">
                    <div class="list-row-main">
                      <div class="list-row-title">Scout Watchdog</div>
                      <div class="list-row-desc">Pengawas binder / memory Xiaomi. Matikan = ga kill pas IPC sesak. <span style="color: var(--on-surface);">Efek: app ga tiba-tiba hilang.</span></div>
                    </div>
                    <label class="md3-switch">
                      <input type="checkbox" :checked="!status.killers.scout_enable" @change="toggleFeature('scout', status.killers.scout_enable)" />
                      <span class="md3-switch-track"><span class="md3-switch-thumb"></span></span>
                    </label>
                  </div>
                  <div class="list-row">
                    <div class="list-row-main">
                      <div class="list-row-title">Smart Power Control (SPC)</div>
                      <div class="list-row-desc">Batasi proses background jadi cuma 2. Matikan = bebas. <span style="color: var(--on-surface);">Efek: service background jalan normal.</span></div>
                    </div>
                    <label class="md3-switch">
                      <input type="checkbox" :checked="!status.killers.spc_enable" @change="toggleFeature('spc', status.killers.spc_enable)" />
                      <span class="md3-switch-track"><span class="md3-switch-thumb"></span></span>
                    </label>
                  </div>
                  <div class="list-row">
                    <div class="list-row-main">
                      <div class="list-row-title">Memory Standard — 5 menit</div>
                      <div class="list-row-desc">Matikan app setelah 5 menit nganggur. Matikan = ga auto-kill. <span style="color: var(--on-surface);">Efek: app nahan lebih lama.</span></div>
                    </div>
                    <label class="md3-switch">
                      <input type="checkbox" :checked="!status.killers.memory_standard_enable" @change="toggleFeature('memstd', status.killers.memory_standard_enable)" />
                      <span class="md3-switch-track"><span class="md3-switch-thumb"></span></span>
                    </label>
                  </div>
                  <div class="list-row">
                    <div class="list-row-main">
                      <div class="list-row-title">Camera Boost Kill</div>
                      <div class="list-row-desc">Pas buka kamera, sistem turunkan ambang kill. Matikan = ambang tetap longgar. <span style="color: var(--on-surface);">Efek: app lain ga kekorban pas foto.</span></div>
                    </div>
                    <label class="md3-switch">
                      <input type="checkbox" :checked="status.killers.camera_boost_adj.includes('999')" @change="toggleFeature('camera', !status.killers.camera_boost_adj.includes('999'))" />
                      <span class="md3-switch-track"><span class="md3-switch-thumb"></span></span>
                    </label>
                  </div>
                  <div v-if="status.caps.pk" class="list-row">
                    <div class="list-row-main">
                      <div class="list-row-title">PowerKeeper Freezer</div>
                      <div class="list-row-desc">Layanan Xiaomi yang bekukan task. Matikan = ga dibekukan. <span style="color: var(--on-surface);">Efek: background task jalan.</span></div>
                    </div>
                    <label class="md3-switch">
                      <input type="checkbox" :checked="status.killers.pk_freezer === 'false'" @change="toggleFeature('pk_freezer', status.killers.pk_freezer !== 'false')" />
                      <span class="md3-switch-track"><span class="md3-switch-thumb"></span></span>
                    </label>
                  </div>
                </div>
              </div>
            </template>

            <!-- Grup: Adaptive -->
            <div v-if="status.caps.samsung || status.caps.oplus || status.caps.transsion">
              <div style="font-size: 11px; font-weight: 700; letter-spacing: 0.3px; color: var(--outline); margin-bottom: 6px; display: flex; align-items: center; gap: 6px;">
                <span style="height: 1px; flex: 1; background: var(--outline-variant);"></span>
                {{ status.caps.samsung ? 'SAMSUNG — OneUI' : status.caps.oplus ? 'OPLUS — ColorOS/OxygenOS' : 'TRANSSION — XOS/HiOS' }}
                <span style="height: 1px; flex: 1; background: var(--outline-variant);"></span>
              </div>
              <div class="list-row">
                <div class="list-row-main">
                  <div class="list-row-title">Adaptive Battery</div>
                  <div class="list-row-desc">{{ adaptiveTunablesDesc }} <span style="color: var(--on-surface);">Efek: app ga di-standby paksa.</span></div>
                </div>
                <label class="md3-switch">
                  <input type="checkbox" :checked="status.stats.is_pacified" @change="toggleFeature('adaptive', !status.stats.is_pacified)" />
                  <span class="md3-switch-track"><span class="md3-switch-thumb"></span></span>
                </label>
              </div>
            </div>

            <div v-if="!status.caps.phantom && !status.caps.freezer && !status.caps.xiaomi && !status.caps.duraspeed && !status.caps.samsung && !status.caps.oplus && !status.caps.transsion" style="text-align: center; padding: 16px; color: var(--on-surface-variant); font-size: 12px;">
              HP ini cuma pakai LMKD + whitelist. Ga ada mesin vendor khusus — udah simpel.
            </div>
          </div>
        </section>
      </template>

      <!-- ==================== LOG ==================== -->
      <template v-else-if="activeTab === 'logs'">
        <section class="md3-card">
          <div class="card-title-row">
            <span class="card-title">Log — bukti kill</span>
            <div style="display: flex; gap: 6px;">
              <button class="btn btn-outline btn-sm" @click="fetchLogs">
                <Icons name="refresh" :size="11" :class="{ 'spin-anim': isRefreshingLogs }" />
                <span>Refresh</span>
              </button>
              <button class="btn btn-outline btn-sm" @click="logsList = []">
                <span>Bersihkan</span>
              </button>
            </div>
          </div>
          <p class="card-desc">
            Kalau app masih ketutup padahal udah Optimized, cek sini ada jejak <span style="color: var(--on-surface);">lmkd / am_kill / powerkeeper / scout{{ status.caps.samsung ? ' / samsung' : '' }}</span> atau ga. Kosong = bagus.
          </p>
          <div class="console-box">
            <div v-if="isRefreshingLogs" style="color: var(--outline); text-align: center; padding: 20px;">Membaca logcat...</div>
            <div v-else-if="logsList.length === 0" style="color: var(--outline); text-align: center; padding: 20px;">
              Belum ada jejak kill — bagus. Coba pakai HP beberapa jam lalu Refresh lagi.
            </div>
            <div v-else v-for="(line, idx) in logsList" :key="idx" class="console-line">{{ line }}</div>
          </div>
        </section>
      </template>
    </main>

    <nav class="md3-navbar">
      <button class="nav-item" :class="{ active: activeTab === 'dashboard' }" @click="activeTab = 'dashboard'">
        <div class="nav-icon-wrapper"><Icons name="zap" :size="18" /></div>
        <span class="nav-label">Ringkasan</span>
      </button>
      <button class="nav-item" :class="{ active: activeTab === 'apps' }" @click="switchTab('apps')">
        <div class="nav-icon-wrapper"><Icons name="apps" :size="18" /></div>
        <span class="nav-label">Aplikasi</span>
      </button>
      <button class="nav-item" :class="{ active: activeTab === 'tunables' }" @click="activeTab = 'tunables'">
        <div class="nav-icon-wrapper"><Icons name="sliders" :size="18" /></div>
        <span class="nav-label">Mesin</span>
      </button>
      <button class="nav-item" :class="{ active: activeTab === 'logs' }" @click="switchTab('logs')">
        <div class="nav-icon-wrapper"><Icons name="terminal" :size="18" /></div>
        <span class="nav-label">Log</span>
      </button>
    </nav>

    <transition name="toast-slide">
      <div v-if="toastMsg" class="toast-pill">
        <Icons name="zap" :size="13" style="color: var(--primary);" />
        <span>{{ toastMsg }}</span>
      </div>
    </transition>
  </div>
</template>

<script setup>
import { ref, computed, onMounted, nextTick } from 'vue'
import Icons from './components/Icons.vue'
import { runBridgeJson, formatPkgName } from './helpers/shell.js'

const activeTab = ref('dashboard')
const isRefreshing = ref(false)
const isApplying = ref(false)
const isLoadingApps = ref(false)
const isRefreshingLogs = ref(false)
const toastMsg = ref('')

const headerSub = computed(() => deviceSub.value)

const deviceSub = computed(() => {
  const d = status.value.device
  if (!d.model) return 'Universal background execution control'
  const oem = d.oem === 'generic' ? '' : ` · ${d.skin}`
  return `${d.model}${oem} (Android ${d.android}, SDK ${d.sdk || ''})`.trim()
})

const oemLabel = computed(() => {
  const d = status.value.device
  if (!d.oem) return 'This device'
  if (d.oem === 'xiaomi') return `Xiaomi · ${d.skin}`
  if (d.oem === 'samsung') return `Samsung · ${d.skin}`
  if (d.oem === 'oplus') return `OPLUS · ${d.skin}`
  if (d.oem === 'transsion') return `Transsion · ${d.skin}`
  return `${d.oem} · ${d.skin}`
})

const dashboardDesc = computed(() => {
  if (status.value.stats.is_pacified) {
    const parts = []
    if (status.value.caps.phantom) parts.push('Phantom')
    if (status.value.caps.freezer) parts.push('Freezer')
    if (status.value.caps.xiaomi) parts.push('Scout, SPC, PowerKeeper')
    if (status.value.caps.duraspeed) parts.push('DuraSpeed')
    if (status.value.caps.samsung || status.value.caps.oplus || status.value.caps.transsion) parts.push('Adaptive')
    parts.push('LMKD')
    return `${parts.join(' + ')} udah dijinakin. Aplikasi dibiarin jalan di background, ga dipotong sesuka sistem.`
  }
  const c = status.value.caps
  if (c.xiaomi) return 'Masih bawaan HyperOS: kalau app nganggur 5 menit atau RAM sesak, bisa dimatiin di belakang.'
  if (c.samsung) return 'Masih bawaan OneUI: adaptive battery bisa standby-in app di background.'
  if (c.oplus) return 'Masih bawaan ColorOS: battery optimization bisa batasi background.'
  if (c.transsion) return 'Masih bawaan XOS: battery manager bisa batasi background.'
  return 'Masih bawaan Android: sistem masih batasi background kalau perlu RAM.'
})

const adaptiveSub = computed(() => {
  if (status.value.caps.samsung) return 'OneUI adaptive battery + standby bucket.'
  if (status.value.caps.oplus) return 'ColorOS standby + battery optimization.'
  if (status.value.caps.transsion) return 'XOS background freeze.'
  return 'OEM adaptive battery.'
})

const adaptiveTunablesDesc = computed(() => {
  if (status.value.caps.samsung) return 'OneUI Device Care — adaptive battery + app standby.'
  if (status.value.caps.oplus) return 'ColorOS/OxygenOS — battery optimization.'
  if (status.value.caps.transsion) return 'XOS/HiOS — auto-start + standby.'
  return 'OEM battery optimization.'
})

const status = ref({
  device: { model: '', android: '', os: '', oem: 'generic', skin: 'AOSP', sdk: 0 },
  ram: { total_mb: 0, used_mb: 0, free_mb: 0, zram_total_mb: 0, zram_used_mb: 0 },
  caps: { phantom: false, freezer: false, duraspeed: false, pk: false, xiaomi: false, samsung: false, transsion: false, oplus: false },
  killers: {
    phantom_limit: '32',
    cached_freezer: 'true',
    duraspeed_disabled: false,
    scout_enable: true,
    spc_enable: true,
    memory_standard_enable: true,
    camera_boost_adj: '400:250:100:50',
    pk_freezer: 'true',
    lmkd_psi_partial: '35',
    lmkd_psi_complete: '70'
  },
  stats: {
    total_apps: 340,
    restricted_apps: 0,
    unrestricted_apps: 340,
    whitelisted_apps: 67,
    available_killers: 1,
    pacified_killers: 0,
    is_pacified: false
  }
})

const appsList = ref([])
const logsList = ref([])
const searchQuery = ref('')
const appFilter = ref('all')
const visibleLimit = ref(30)

const ramPercent = computed(() => {
  if (!status.value.ram.total_mb) return 0
  return Math.min(100, Math.round((status.value.ram.used_mb / status.value.ram.total_mb) * 100))
})

const zramPercent = computed(() => {
  if (!status.value.ram.zram_total_mb) return 0
  return Math.min(100, Math.round((status.value.ram.zram_used_mb / status.value.ram.zram_total_mb) * 100))
})

const filteredApps = computed(() => {
  let list = appsList.value
  if (appFilter.value === 'restricted') {
    list = list.filter(a => a.bgControl !== 'noRestrict')
  } else if (appFilter.value === 'unrestricted') {
    list = list.filter(a => a.bgControl === 'noRestrict')
  } else if (appFilter.value === 'user') {
    list = list.filter(a => a.isUser)
  }
  if (searchQuery.value.trim()) {
    const q = searchQuery.value.toLowerCase().trim()
    list = list.filter(a => a.pkg.toLowerCase().includes(q) || formatAppTitle(a.pkg).toLowerCase().includes(q))
  }
  return list
})

const visibleApps = computed(() => filteredApps.value.slice(0, visibleLimit.value))

function formatAppTitle(pkg) { return formatPkgName(pkg) }

function showToast(msg) {
  toastMsg.value = msg
  setTimeout(() => { toastMsg.value = '' }, 2200)
}

function formatMb(mb) {
  if (!mb) return '0 MB'
  if (mb >= 1024) return (mb / 1024).toFixed(1) + ' GB'
  return `${mb} MB`
}

async function fetchStatus() {
  isRefreshing.value = true
  try {
    const res = await runBridgeJson('status')
    if (res && res.device) status.value = res
  } catch (e) { console.error(e) } finally { isRefreshing.value = false }
}

async function pacifyAll() {
  isApplying.value = true
  status.value.stats.is_pacified = true
  if (status.value.caps.phantom) status.value.killers.phantom_limit = '2147483647'
  if (status.value.caps.freezer) status.value.killers.cached_freezer = 'false'
  if (status.value.caps.duraspeed) status.value.killers.duraspeed_disabled = true
  if (status.value.caps.xiaomi) {
    status.value.killers.scout_enable = false
    status.value.killers.spc_enable = false
    status.value.killers.memory_standard_enable = false
    status.value.killers.camera_boost_adj = '999:999:999:999'
    if (status.value.caps.pk) status.value.killers.pk_freezer = 'false'
  }
  status.value.killers.lmkd_psi_partial = '250'
  showToast('Menerapkan...')
  try {
    await runBridgeJson('pacify')
    showToast('Udah optimal — app dibiarin jalan')
    await fetchStatus()
    if (appsList.value.length > 0) appsList.value.forEach(a => a.bgControl = 'noRestrict')
  } catch (e) { showToast('Gagal menerapkan') } finally { isApplying.value = false }
}

async function restoreStock() {
  isApplying.value = true
  status.value.stats.is_pacified = false
  if (status.value.caps.phantom) status.value.killers.phantom_limit = '32'
  if (status.value.caps.freezer) status.value.killers.cached_freezer = 'true'
  if (status.value.caps.duraspeed) status.value.killers.duraspeed_disabled = false
  if (status.value.caps.xiaomi) {
    status.value.killers.scout_enable = true
    status.value.killers.spc_enable = true
    status.value.killers.memory_standard_enable = true
    status.value.killers.camera_boost_adj = '400:250:100:50'
    if (status.value.caps.pk) status.value.killers.pk_freezer = 'true'
  }
  status.value.killers.lmkd_psi_partial = '35'
  showToast('Mengembalikan bawaan...')
  try {
    await runBridgeJson('restore_stock')
    showToast('Balik ke bawaan pabrik')
    await fetchStatus()
  } catch (e) { showToast('Gagal mengembalikan') } finally { isApplying.value = false }
}

async function fetchApps() {
  isLoadingApps.value = true
  try {
    const res = await runBridgeJson('apps')
    if (Array.isArray(res)) appsList.value = res
  } catch (e) { console.error(e) } finally { isLoadingApps.value = false }
}

async function setAllUnrestricted() {
  showToast('Membebaskan yang tampil...')
  appsList.value.forEach(a => { if (filteredApps.value.includes(a)) a.bgControl = 'noRestrict' })
  try {
    await runBridgeJson('set_all_unrestricted')
    showToast('Yang tampil udah bebas')
    fetchStatus()
  } catch (e) { showToast('Gagal') }
}

async function whitelistUserApps() {
  showToast('Whitelist yang kamu install...')
  appsList.value.forEach(a => { if (a.isUser) a.whitelisted = true })
  try {
    await runBridgeJson('pacify')
    showToast('User apps di-whitelist')
    fetchStatus()
  } catch (e) { showToast('Gagal') }
}

async function toggleAppRestriction(app) {
  const nextMode = app.bgControl === 'noRestrict' ? 'miuiAuto' : 'noRestrict'
  app.bgControl = nextMode
  try {
    runBridgeJson('set_app', app.pkg, nextMode)
    showToast(`${formatAppTitle(app.pkg)}: ${nextMode === 'noRestrict' ? 'Bebas' : 'Dibatasi'}`)
  } catch (e) { app.bgControl = nextMode === 'noRestrict' ? 'miuiAuto' : 'noRestrict' }
}

async function toggleAppWhitelist(app) {
  const nextVal = !app.whitelisted
  app.whitelisted = nextVal
  try {
    runBridgeJson('whitelist_app', app.pkg, nextVal ? '1' : '0')
    showToast(`${formatAppTitle(app.pkg)}: ${nextVal ? 'Whitelist' : 'Default'}`)
  } catch (e) { app.whitelisted = !nextVal }
}

async function toggleFeature(key, enable) {
  if (key === 'phantom') status.value.killers.phantom_limit = enable ? '2147483647' : '32'
  else if (key === 'freezer') status.value.killers.cached_freezer = enable ? 'false' : 'true'
  else if (key === 'duraspeed') status.value.killers.duraspeed_disabled = enable
  else if (key === 'scout') status.value.killers.scout_enable = !enable
  else if (key === 'spc') status.value.killers.spc_enable = !enable
  else if (key === 'memstd') status.value.killers.memory_standard_enable = !enable
  else if (key === 'camera') status.value.killers.camera_boost_adj = enable ? '999:999:999:999' : '400:250:100:50'
  else if (key === 'lmkd') status.value.killers.lmkd_psi_partial = enable ? '250' : '35'
  else if (key === 'pk_freezer') status.value.killers.pk_freezer = enable ? 'false' : 'true'
  try {
    const res = await runBridgeJson('toggle', key, enable ? '1' : '0')
    if (res && res.status === 'skipped') { showToast(`${key}: ga ada di HP ini`); await fetchStatus() }
    else showToast(`${key} ${enable ? 'dimatikan' : 'dinyalakan'}`)
  } catch (e) { showToast(`Gagal ${key}`) }
}

async function fetchLogs() {
  isRefreshingLogs.value = true
  try {
    const res = await runBridgeJson('logs')
    if (Array.isArray(res)) logsList.value = res
  } catch (e) { console.error(e) } finally { isRefreshingLogs.value = false }
}

function switchTab(tab) {
  activeTab.value = tab
  nextTick(() => { document.querySelector('.content-area')?.scrollTo({ top: 0, behavior: 'instant' }) })
  if (tab === 'apps' && appsList.value.length === 0) fetchApps()
  else if (tab === 'logs' && logsList.value.length === 0) fetchLogs()
}

onMounted(() => { fetchStatus() })
</script>
