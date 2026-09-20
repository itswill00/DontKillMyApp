<template>
  <div class="app-shell">
    <header class="page-header">
      <div style="min-width: 0; flex: 1;">
        <div class="page-header-title">{{ headerTitle }}</div>
        <div class="page-header-sub truncate-text">{{ headerSub }}</div>
      </div>
      <div style="display: flex; align-items: center; gap: 8px; flex-shrink: 0;">
        <span
          v-if="activeTab === 'dashboard'"
          class="badge-pill active"
          style="cursor: pointer; user-select: none;"
          @click="fetchStatus"
          title="Click to refresh status"
        >
          {{ status.stats.is_pacified ? 'Optimized' : 'Stock' }} · v1.0.0
        </span>
        <span
          v-else-if="activeTab === 'apps'"
          class="badge-pill active"
        >
          {{ filteredApps.length }} apps
        </span>
        <span
          v-else-if="activeTab === 'tunables'"
          class="badge-pill active"
        >
          {{ status.stats.pacified_killers }}/{{ status.stats.available_killers }}
        </span>
        <button
          v-else-if="activeTab === 'logs'"
          class="btn btn-outline btn-sm"
          @click="fetchLogs"
          title="Refresh log buffer"
        >
          <Icons name="refresh" :size="12" :class="{ 'spin-anim': isRefreshingLogs }" />
          <span>Refresh</span>
        </button>
      </div>
    </header>

    <main class="content-area">
      <template v-if="activeTab === 'dashboard'">
        <section class="md3-card">
          <div class="card-title-row">
            <span class="card-title">Background Execution</span>
            <span class="status-indicator" :class="{ active: status.stats.is_pacified }">
              {{ status.stats.is_pacified ? 'Active' : 'Stock OEM' }}
            </span>
          </div>

          <p class="card-desc">
            {{ dashboardDesc }}
          </p>

          <div style="display: flex; gap: 8px; margin-top: 4px;">
            <button
              class="btn btn-primary"
              style="flex: 1;"
              :disabled="isApplying"
              @click="pacifyAll"
            >
              <span>{{ isApplying ? 'Applying...' : 'Apply Optimization' }}</span>
            </button>
            <button
              class="btn btn-outline"
              :disabled="isApplying"
              @click="restoreStock"
            >
              <span>Restore Defaults</span>
            </button>
          </div>
          <div v-if="status.device.oem" style="font-size: 10.5px; color: var(--outline); font-variant-numeric: tabular-nums;">
            {{ oemLabel }} · {{ status.stats.available_killers }} killers available, {{ status.stats.pacified_killers }} optimized
          </div>
        </section>

        <section class="md3-card">
          <div class="card-title-row">
            <span class="card-title">Memory</span>
            <span class="card-extra">{{ formatMb(status.ram.used_mb) }} / {{ formatMb(status.ram.total_mb) }}</span>
          </div>

          <div style="display: flex; flex-direction: column; gap: 10px;">
            <div>
              <div style="display: flex; justify-content: space-between; font-size: 11px; margin-bottom: 4px;">
                <span style="color: var(--on-surface-variant);">Physical RAM</span>
                <span style="font-variant-numeric: tabular-nums;">{{ ramPercent }}% ({{ formatMb(status.ram.free_mb) }} available)</span>
              </div>
              <div class="bar-track">
                <div class="bar-fill" :style="{ width: ramPercent + '%' }"></div>
              </div>
            </div>

            <div>
              <div style="display: flex; justify-content: space-between; font-size: 11px; margin-bottom: 4px;">
                <span style="color: var(--on-surface-variant);">ZRAM Swap</span>
                <span style="font-variant-numeric: tabular-nums;">{{ zramPercent }}% ({{ formatMb(status.ram.zram_used_mb) }} / {{ formatMb(status.ram.zram_total_mb) }})</span>
              </div>
              <div class="bar-track">
                <div class="bar-fill" :style="{ width: zramPercent + '%', background: 'var(--secondary)' }"></div>
              </div>
            </div>
          </div>
        </section>

        <section class="md3-card">
          <div class="card-title-row">
            <span class="card-title">Subsystem Status</span>
            <span class="card-extra">{{ status.stats.unrestricted_apps }} unrestricted</span>
          </div>

          <div class="status-list">
            <!-- Universal: Always present -->
            <div v-if="status.caps.phantom" class="status-row">
              <div class="status-col">
                <div class="status-name">Phantom Process Killer</div>
                <div class="status-sub">AOSP 12+ child process limit</div>
              </div>
              <span class="status-badge" :class="{ ok: status.killers.phantom_limit === '2147483647' }">
                {{ status.killers.phantom_limit === '2147483647' ? 'Unlimited' : status.killers.phantom_limit }}
              </span>
            </div>

            <div v-if="status.caps.freezer" class="status-row">
              <div class="status-col">
                <div class="status-name">Cached Apps Freezer</div>
                <div class="status-sub">AOSP 14+ cgroup v2 task freezer</div>
              </div>
              <span class="status-badge" :class="{ ok: status.killers.cached_freezer === 'false' }">
                {{ status.killers.cached_freezer === 'false' ? 'Disabled' : 'Active' }}
              </span>
            </div>

            <div class="status-row">
              <div class="status-col">
                <div class="status-name">LMKD PSI Stall Threshold</div>
                <div class="status-sub">Low memory pressure threshold</div>
              </div>
              <span class="status-badge" :class="{ ok: status.killers.lmkd_psi_partial !== '35' }">
                {{ status.killers.lmkd_psi_partial }} ms
              </span>
            </div>

            <!-- Xiaomi-only -->
            <div v-if="status.caps.xiaomi" class="status-row">
              <div class="status-col">
                <div class="status-name">Xiaomi Scout Watchdog</div>
                <div class="status-sub">Binder thread and buffer monitor</div>
              </div>
              <span class="status-badge" :class="{ ok: !status.killers.scout_enable }">
                {{ !status.killers.scout_enable ? 'Disabled' : 'Active' }}
              </span>
            </div>

            <div v-if="status.caps.xiaomi" class="status-row">
              <div class="status-col">
                <div class="status-name">Smart Power Control (SPC)</div>
                <div class="status-sub">Background task limiter</div>
              </div>
              <span class="status-badge" :class="{ ok: !status.killers.spc_enable }">
                {{ !status.killers.spc_enable ? 'Disabled' : 'Active' }}
              </span>
            </div>

            <div v-if="status.caps.xiaomi" class="status-row">
              <div class="status-col">
                <div class="status-name">Memory Standard</div>
                <div class="status-sub">300-second standby timeout</div>
              </div>
              <span class="status-badge" :class="{ ok: !status.killers.memory_standard_enable }">
                {{ !status.killers.memory_standard_enable ? 'Disabled' : '300s' }}
              </span>
            </div>

            <div v-if="status.caps.pk" class="status-row">
              <div class="status-col">
                <div class="status-name">PowerKeeper Freezer</div>
                <div class="status-sub">FrozenControlNewStatus</div>
              </div>
              <span class="status-badge" :class="{ ok: status.killers.pk_freezer === 'false' }">
                {{ status.killers.pk_freezer === 'false' ? 'Disabled' : 'Active' }}
              </span>
            </div>

            <!-- MediaTek -->
            <div v-if="status.caps.duraspeed" class="status-row">
              <div class="status-col">
                <div class="status-name">MediaTek DuraSpeed</div>
                <div class="status-sub">Vendor background reclamation</div>
              </div>
              <span class="status-badge" :class="{ ok: status.killers.duraspeed_disabled }">
                {{ status.killers.duraspeed_disabled ? 'Disabled' : 'Active' }}
              </span>
            </div>

            <!-- Samsung / OPlus / Transsion adaptive (shown as single unified row) -->
            <div v-if="status.caps.samsung || status.caps.oplus || status.caps.transsion" class="status-row">
              <div class="status-col">
                <div class="status-name">Adaptive Battery</div>
                <div class="status-sub">{{ adaptiveSub }}</div>
              </div>
              <span class="status-badge" :class="{ ok: status.stats.is_pacified }">
                {{ status.stats.is_pacified ? 'Disabled' : 'Active' }}
              </span>
            </div>
          </div>
        </section>
      </template>

      <template v-else-if="activeTab === 'apps'">
        <section class="md3-card">
          <div class="card-title-row">
            <span class="card-title">Application Management</span>
            <span class="card-extra">{{ filteredApps.length }} apps</span>
          </div>

          <div style="display: flex; gap: 8px;">
            <button class="btn btn-primary btn-sm" style="flex: 1;" @click="setAllUnrestricted">
              Set All Unrestricted
            </button>
            <button class="btn btn-outline btn-sm" @click="whitelistUserApps">
              Whitelist User
            </button>
          </div>

          <div class="search-box">
            <Icons name="search" :size="13" style="color: var(--outline);" />
            <input
              type="text"
              v-model="searchQuery"
              placeholder="Search package..."
            />
            <span v-if="searchQuery" @click="searchQuery = ''" style="cursor: pointer; color: var(--outline);">
              <Icons name="x" :size="12" />
            </span>
          </div>

          <div class="chips-row">
            <div
              class="chip"
              :class="{ active: appFilter === 'user' }"
              @click="appFilter = 'user'"
            >
              User Apps ({{ appsList.filter(a => a.isUser).length }})
            </div>
            <div
              class="chip"
              :class="{ active: appFilter === 'all' }"
              @click="appFilter = 'all'"
            >
              All ({{ appsList.length }})
            </div>
            <div
              class="chip"
              :class="{ active: appFilter === 'restricted' }"
              @click="appFilter = 'restricted'"
            >
              Restricted ({{ appsList.filter(a => a.bgControl !== 'noRestrict').length }})
            </div>
            <div
              class="chip"
              :class="{ active: appFilter === 'unrestricted' }"
              @click="appFilter = 'unrestricted'"
            >
              Unrestricted ({{ appsList.filter(a => a.bgControl === 'noRestrict').length }})
            </div>
          </div>
        </section>

        <section class="md3-card">
          <div v-if="isLoadingApps" style="text-align: center; padding: 20px; color: var(--on-surface-variant);">
            <Icons name="refresh" :size="18" class="spin-anim" />
            <div style="margin-top: 8px; font-size: 12px;">Loading application list...</div>
          </div>

          <div v-else-if="filteredApps.length === 0" style="text-align: center; padding: 24px; color: var(--on-surface-variant);">
            <div style="font-size: 12.5px;">No applications match filter</div>
            <div style="font-size: 11px; color: var(--outline); margin-top: 2px;">Try switching to 'All' or clearing search</div>
          </div>

          <div v-else style="display: flex; flex-direction: column;">
            <div
              v-for="app in visibleApps"
              :key="app.pkg"
              class="list-row"
            >
              <div class="list-row-main">
                <div class="list-row-title">{{ formatAppTitle(app.pkg) }}</div>
                <!-- deslop-ignore-next-line 34 -->
                <div style="font-size: 10.5px; color: var(--outline); font-family: var(--font-mono);">{{ app.pkg }}</div>
                <div style="display: flex; gap: 6px; margin-top: 4px; align-items: center;">
                  <span
                    class="status-indicator"
                    :class="{ active: app.bgControl === 'noRestrict' }"
                    style="cursor: pointer; font-size: 10.5px;"
                    @click="toggleAppRestriction(app)"
                  >
                    {{ app.bgControl === 'noRestrict' ? 'No Restrictions' : 'Restricted' }}
                  </span>
                  <span style="color: var(--outline);">·</span>
                  <span
                    class="status-indicator"
                    :class="{ active: app.whitelisted }"
                    style="cursor: pointer; font-size: 10.5px;"
                    @click="toggleAppWhitelist(app)"
                  >
                    {{ app.whitelisted ? 'Whitelisted' : 'Optimized' }}
                  </span>
                </div>
              </div>

              <label class="md3-switch">
                <input
                  type="checkbox"
                  :checked="app.bgControl === 'noRestrict'"
                  @change="toggleAppRestriction(app)"
                />
                <span class="md3-switch-track">
                  <span class="md3-switch-thumb"></span>
                </span>
              </label>
            </div>

            <div v-if="filteredApps.length > visibleLimit" style="text-align: center; padding: 12px 0 4px 0;">
              <button class="btn btn-outline btn-sm btn-block" @click="visibleLimit += 35">
                Load More (Showing {{ visibleLimit }} of {{ filteredApps.length }})
              </button>
            </div>
          </div>
        </section>
      </template>

      <template v-else-if="activeTab === 'tunables'">
        <section class="md3-card">
          <div class="card-title-row">
            <span class="card-title">System Settings</span>
            <span class="card-extra">{{ status.stats.pacified_killers }}/{{ status.stats.available_killers }} active</span>
          </div>

          <div style="display: flex; flex-direction: column;">
            <!-- Phantom (AOSP 12+) -->
            <div v-if="status.caps.phantom" class="list-row">
              <div class="list-row-main">
                <div class="list-row-title">Phantom Process Killer</div>
                <div class="list-row-desc">ActivityManager 32-child process limit for apps and workers.</div>
              </div>
              <label class="md3-switch">
                <input
                  type="checkbox"
                  :checked="status.killers.phantom_limit === '2147483647'"
                  @change="toggleFeature('phantom', status.killers.phantom_limit !== '2147483647')"
                />
                <span class="md3-switch-track">
                  <span class="md3-switch-thumb"></span>
                </span>
              </label>
            </div>

            <!-- Freezer (AOSP 14+) -->
            <div v-if="status.caps.freezer" class="list-row">
              <div class="list-row-main">
                <div class="list-row-title">Cached Apps Freezer</div>
                <div class="list-row-desc">cgroup v2 task freezer for background sockets and workers.</div>
              </div>
              <label class="md3-switch">
                <input
                  type="checkbox"
                  :checked="status.killers.cached_freezer === 'false'"
                  @change="toggleFeature('freezer', status.killers.cached_freezer !== 'false')"
                />
                <span class="md3-switch-track">
                  <span class="md3-switch-thumb"></span>
                </span>
              </label>
            </div>

            <!-- LMKD always available -->
            <div class="list-row">
              <div class="list-row-main">
                <div class="list-row-title">Relax LMKD PSI Stall (250ms)</div>
                <div class="list-row-desc">Prevents premature memory kills under brief pressure spikes.</div>
              </div>
              <label class="md3-switch">
                <input
                  type="checkbox"
                  :checked="status.killers.lmkd_psi_partial !== '35'"
                  @change="toggleFeature('lmkd', status.killers.lmkd_psi_partial === '35')"
                />
                <span class="md3-switch-track">
                  <span class="md3-switch-thumb"></span>
                </span>
              </label>
            </div>

            <!-- MediaTek -->
            <div v-if="status.caps.duraspeed" class="list-row">
              <div class="list-row-main">
                <div class="list-row-title">MediaTek DuraSpeed</div>
                <div class="list-row-desc">MediaTek memory reclamation daemon.</div>
              </div>
              <label class="md3-switch">
                <input
                  type="checkbox"
                  :checked="status.killers.duraspeed_disabled"
                  @change="toggleFeature('duraspeed', !status.killers.duraspeed_disabled)"
                />
                <span class="md3-switch-track">
                  <span class="md3-switch-thumb"></span>
                </span>
              </label>
            </div>

            <!-- Xiaomi-only rows -->
            <template v-if="status.caps.xiaomi">
            <div class="list-row">
              <div class="list-row-main">
                <div class="list-row-title">Xiaomi Scout Watchdog</div>
                <div class="list-row-desc">HyperOS process watchdog monitoring IPC memory load.</div>
              </div>
              <label class="md3-switch">
                <input
                  type="checkbox"
                  :checked="!status.killers.scout_enable"
                  @change="toggleFeature('scout', status.killers.scout_enable)"
                />
                <span class="md3-switch-track">
                  <span class="md3-switch-thumb"></span>
                </span>
              </label>
            </div>

            <div class="list-row">
              <div class="list-row-main">
                <div class="list-row-title">Smart Power Control (SPC)</div>
                <div class="list-row-desc">Restricts protected background processes to 2 instances.</div>
              </div>
              <label class="md3-switch">
                <input
                  type="checkbox"
                  :checked="!status.killers.spc_enable"
                  @change="toggleFeature('spc', status.killers.spc_enable)"
                />
                <span class="md3-switch-track">
                  <span class="md3-switch-thumb"></span>
                </span>
              </label>
            </div>

            <div class="list-row">
              <div class="list-row-main">
                <div class="list-row-title">Memory Standard (300s Standby)</div>
                <div class="list-row-desc">Terminates background apps after 5 minutes of device idle.</div>
              </div>
              <label class="md3-switch">
                <input
                  type="checkbox"
                  :checked="!status.killers.memory_standard_enable"
                  @change="toggleFeature('memstd', status.killers.memory_standard_enable)"
                />
                <span class="md3-switch-track">
                  <span class="md3-switch-thumb"></span>
                </span>
              </label>
            </div>

            <div class="list-row">
              <div class="list-row-main">
                <div class="list-row-title">Camera Boost Kill Adj</div>
                <div class="list-row-desc">Lowers OOM score threshold when camera is opened.</div>
              </div>
              <label class="md3-switch">
                <input
                  type="checkbox"
                  :checked="status.killers.camera_boost_adj.includes('999')"
                  @change="toggleFeature('camera', !status.killers.camera_boost_adj.includes('999'))"
                />
                <span class="md3-switch-track">
                  <span class="md3-switch-thumb"></span>
                </span>
              </label>
            </div>

            <div v-if="status.caps.pk" class="list-row">
              <div class="list-row-main">
                <div class="list-row-title">PowerKeeper Freezer</div>
                <div class="list-row-desc">PowerKeeper background task suspension service.</div>
              </div>
              <label class="md3-switch">
                <input
                  type="checkbox"
                  :checked="status.killers.pk_freezer === 'false'"
                  @change="toggleFeature('pk_freezer', status.killers.pk_freezer !== 'false')"
                />
                <span class="md3-switch-track">
                  <span class="md3-switch-thumb"></span>
                </span>
              </label>
            </div>
            </template>

            <!-- Samsung / OPlus / Transsion adaptive -->
            <div v-if="status.caps.samsung || status.caps.oplus || status.caps.transsion" class="list-row">
              <div class="list-row-main">
                <div class="list-row-title">Adaptive Battery</div>
                <div class="list-row-desc">{{ adaptiveTunablesDesc }}</div>
              </div>
              <label class="md3-switch">
                <input
                  type="checkbox"
                  :checked="status.stats.is_pacified"
                  @change="toggleFeature('adaptive', !status.stats.is_pacified)"
                />
                <span class="md3-switch-track">
                  <span class="md3-switch-thumb"></span>
                </span>
              </label>
            </div>

            <div v-if="!status.caps.phantom && !status.caps.freezer && !status.caps.xiaomi && !status.caps.duraspeed && !status.caps.samsung && !status.caps.oplus && !status.caps.transsion" style="text-align: center; padding: 16px; color: var(--on-surface-variant); font-size: 12px;">
              All killers are managed via Memory and deviceidle; no vendor-specific toggles on this device.
            </div>
          </div>
        </section>
      </template>

      <template v-else-if="activeTab === 'logs'">
        <section class="md3-card">
          <div class="card-title-row">
            <span class="card-title">Kill Events</span>
            <div style="display: flex; gap: 6px;">
              <button class="btn btn-outline btn-sm" @click="fetchLogs">
                <Icons name="refresh" :size="11" :class="{ 'spin-anim': isRefreshingLogs }" />
                <span>Refresh</span>
              </button>
              <button class="btn btn-outline btn-sm" @click="logsList = []">
                <span>Clear</span>
              </button>
            </div>
          </div>

          <p class="card-desc">
            Filtered from logcat buffer: lmkd, am_kill, powerkeeper, scout, phantom{{ status.caps.samsung ? ', samsung' : '' }}.
          </p>

          <div class="console-box">
            <div v-if="isRefreshingLogs" style="color: var(--outline); text-align: center; padding: 20px;">
              Reading logcat traces...
            </div>
            <div v-else-if="logsList.length === 0" style="color: var(--outline); text-align: center; padding: 20px;">
              No kill events recorded in buffer.
            </div>
            <div
              v-else
              v-for="(line, idx) in logsList"
              :key="idx"
              class="console-line"
            >
              {{ line }}
            </div>
          </div>
        </section>
      </template>
    </main>

    <nav class="md3-navbar">
      <button class="nav-item" :class="{ active: activeTab === 'dashboard' }" @click="activeTab = 'dashboard'">
        <div class="nav-icon-wrapper">
          <Icons name="zap" :size="18" />
        </div>
        <span class="nav-label">Dashboard</span>
      </button>

      <button class="nav-item" :class="{ active: activeTab === 'apps' }" @click="switchTab('apps')">
        <div class="nav-icon-wrapper">
          <Icons name="apps" :size="18" />
        </div>
        <span class="nav-label">Apps</span>
      </button>

      <button class="nav-item" :class="{ active: activeTab === 'tunables' }" @click="activeTab = 'tunables'">
        <div class="nav-icon-wrapper">
          <Icons name="sliders" :size="18" />
        </div>
        <span class="nav-label">Tunables</span>
      </button>

      <button class="nav-item" :class="{ active: activeTab === 'logs' }" @click="switchTab('logs')">
        <div class="nav-icon-wrapper">
          <Icons name="terminal" :size="18" />
        </div>
        <span class="nav-label">Logs</span>
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

const headerTitle = computed(() => {
  switch (activeTab.value) {
    case 'apps': return 'Applications'
    case 'tunables': return 'System Tunables'
    case 'logs': return 'Activity Logs'
    default: return 'DontKillMyApp'
  }
})

const headerSub = computed(() => {
  switch (activeTab.value) {
    case 'apps': return 'Per-app background execution permissions'
    case 'tunables': return tunablesSub.value
    case 'logs': return 'Process termination traces from logcat'
    default: return deviceSub.value
  }
})

const tunablesSub = computed(() => {
  const c = status.value.caps
  if (c.xiaomi) return 'HyperOS & Android killers on this device'
  if (c.samsung) return 'OneUI & Android killers on this device'
  if (c.oplus) return 'ColorOS & Android killers on this device'
  if (c.transsion) return 'XOS & Android killers on this device'
  return 'Android background killers on this device'
})

const deviceSub = computed(() => {
  const d = status.value.device
  if (!d.model) return 'Universal background execution control'
  const oem = d.oem === 'generic' ? '' : ` · ${d.skin}`
  return `${d.model}${oem} (Android ${d.android}, SDK ${d.sdk || ''})`.trim()
})

const oemLabel = computed(() => {
  const d = status.value.device
  if (!d.oem) return 'This device'
  if (d.oem === 'xiaomi') return `Xiaomi • ${d.skin}`
  if (d.oem === 'samsung') return `Samsung • ${d.skin}`
  if (d.oem === 'oplus') return `OPLUS • ${d.skin}`
  if (d.oem === 'transsion') return `Transsion • ${d.skin}`
  return `${d.oem} • ${d.skin}`
})

const dashboardDesc = computed(() => {
  if (status.value.stats.is_pacified) {
    const parts = []
    if (status.value.caps.phantom) parts.push('Phantom limits')
    if (status.value.caps.freezer) parts.push('Cached freezer')
    if (status.value.caps.xiaomi) parts.push('Scout, SPC, Memory Standard, PowerKeeper')
    if (status.value.caps.duraspeed) parts.push('DuraSpeed')
    if (status.value.caps.samsung || status.value.caps.oplus || status.value.caps.transsion) parts.push('Adaptive Battery')
    parts.push('LMKD pressure')
    return `${parts.join(', ')} are pacified. Apps continue running in background.`
  }
  const c = status.value.caps
  if (c.xiaomi) return 'Default HyperOS background management is active. Inactive processes may be terminated by watchdogs after 5 minutes.'
  if (c.samsung) return 'OneUI adaptive battery and phantom limits may suspend background apps.'
  if (c.oplus) return 'ColorOS battery optimization may restrict background execution.'
  if (c.transsion) return 'XOS battery manager may restrict background execution.'
  return 'Default Android background management is active. Background processes may be limited.'
})

const adaptiveSub = computed(() => {
  if (status.value.caps.samsung) return 'OneUI adaptive battery + standby bucket throttling.'
  if (status.value.caps.oplus) return 'ColorOS standby and battery optimization.'
  if (status.value.caps.transsion) return 'XOS background freeze policy.'
  return 'OEM adaptive battery.'
})

const adaptiveTunablesDesc = computed(() => {
  if (status.value.caps.samsung) return 'OneUI Device Care adaptive battery and app standby.'
  if (status.value.caps.oplus) return 'ColorOS/OxygenOS battery optimization.'
  if (status.value.caps.transsion) return 'XOS/HiOS auto-start and standby policy.'
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
const appFilter = ref('user')
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

const visibleApps = computed(() => {
  return filteredApps.value.slice(0, visibleLimit.value)
})

function formatAppTitle(pkg) {
  return formatPkgName(pkg)
}

function showToast(msg) {
  toastMsg.value = msg
  setTimeout(() => {
    toastMsg.value = ''
  }, 2200)
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
    if (res && res.device) {
      status.value = res
    }
  } catch (e) {
    console.error(e)
  } finally {
    isRefreshing.value = false
  }
}

async function pacifyAll() {
  isApplying.value = true
  // Optimistic update only for available killers
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
  showToast('Applying optimizations...')

  try {
    await runBridgeJson('pacify')
    showToast('Optimizations applied')
    await fetchStatus()
    if (appsList.value.length > 0) {
      appsList.value.forEach(a => a.bgControl = 'noRestrict')
    }
  } catch (e) {
    showToast('Failed to apply optimizations')
  } finally {
    isApplying.value = false
  }
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
  showToast('Restoring OEM defaults...')

  try {
    await runBridgeJson('restore_stock')
    showToast('Defaults restored')
    await fetchStatus()
  } catch (e) {
    showToast('Failed to restore defaults')
  } finally {
    isApplying.value = false
  }
}

async function fetchApps() {
  isLoadingApps.value = true
  try {
    const res = await runBridgeJson('apps')
    if (Array.isArray(res)) {
      appsList.value = res
    }
  } catch (e) {
    console.error(e)
  } finally {
    isLoadingApps.value = false
  }
}

async function setAllUnrestricted() {
  showToast('Updating applications...')
  appsList.value.forEach(a => a.bgControl = 'noRestrict')
  try {
    await runBridgeJson('set_all_unrestricted')
    showToast('All apps set to unrestricted')
    fetchStatus()
  } catch (e) {
    showToast('Failed to update apps')
  }
}

async function whitelistUserApps() {
  showToast('Whitelisting user apps...')
  appsList.value.forEach(a => {
    if (a.isUser) a.whitelisted = true
  })
  try {
    await runBridgeJson('pacify')
    showToast('User apps whitelisted')
    fetchStatus()
  } catch (e) {
    showToast('Failed to whitelist apps')
  }
}

async function toggleAppRestriction(app) {
  const nextMode = app.bgControl === 'noRestrict' ? 'miuiAuto' : 'noRestrict'
  app.bgControl = nextMode
  try {
    runBridgeJson('set_app', app.pkg, nextMode)
    showToast(`${formatAppTitle(app.pkg)}: ${nextMode === 'noRestrict' ? 'Unrestricted' : 'Default'}`)
  } catch (e) {
    app.bgControl = nextMode === 'noRestrict' ? 'miuiAuto' : 'noRestrict'
  }
}

async function toggleAppWhitelist(app) {
  const nextVal = !app.whitelisted
  app.whitelisted = nextVal
  try {
    runBridgeJson('whitelist_app', app.pkg, nextVal ? '1' : '0')
    showToast(`${formatAppTitle(app.pkg)}: ${nextVal ? 'Whitelisted' : 'Default'}`)
  } catch (e) {
    app.whitelisted = !nextVal
  }
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
    if (res && res.status === 'skipped') {
      showToast(`${key}: not available on this device`)
      await fetchStatus()
    } else {
      showToast(`Updated ${key}`)
    }
  } catch (e) {
    showToast(`Failed to toggle ${key}`)
  }
}

async function fetchLogs() {
  isRefreshingLogs.value = true
  try {
    const res = await runBridgeJson('logs')
    if (Array.isArray(res)) {
      logsList.value = res
    }
  } catch (e) {
    console.error(e)
  } finally {
    isRefreshingLogs.value = false
  }
}

function switchTab(tab) {
  activeTab.value = tab
  nextTick(() => {
    document.querySelector('.content-area')?.scrollTo({ top: 0, behavior: 'instant' })
  })
  if (tab === 'apps' && appsList.value.length === 0) {
    fetchApps()
  } else if (tab === 'logs' && logsList.value.length === 0) {
    fetchLogs()
  }
}

onMounted(() => {
  fetchStatus()
})
</script>
