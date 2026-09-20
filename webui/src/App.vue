<template>
  <div class="app-shell">
    <!-- Header -->
    <header class="page-header">
      <div style="min-width: 0; flex: 1;">
        <div class="page-header-title">DontKillMyApp</div>
        <div class="page-header-sub truncate-text">
          {{ status.device.model ? `${status.device.model} • HyperOS ${status.device.os} (Android ${status.device.android})` : 'HyperOS Background Pacifier' }}
        </div>
      </div>
      <div style="display: flex; align-items: center; gap: 8px; flex-shrink: 0;">
        <span class="badge-pill" :class="status.stats.is_pacified ? 'success' : 'danger'">
          <Icons :name="status.stats.is_pacified ? 'shield-check' : 'shield-alert'" :size="11" />
          <span>{{ status.stats.is_pacified ? 'Protected' : 'Restricted' }}</span>
        </span>
        <span class="badge-pill" style="cursor: pointer; user-select: none;" @click="fetchStatus" title="Refresh">
          <Icons name="refresh" :size="11" :class="{ 'spin-anim': isRefreshing }" />
          <span>v1.0.0</span>
        </span>
      </div>
    </header>

    <!-- Navigation Tabs -->
    <div class="tabs-container">
      <button class="tab-btn" :class="{ active: activeTab === 'dashboard' }" @click="activeTab = 'dashboard'">
        <Icons name="zap" :size="12" />
        <span>Dashboard</span>
      </button>
      <button class="tab-btn" :class="{ active: activeTab === 'apps' }" @click="switchTab('apps')">
        <Icons name="apps" :size="12" />
        <span>Apps Manager</span>
      </button>
      <button class="tab-btn" :class="{ active: activeTab === 'tunables' }" @click="activeTab = 'tunables'">
        <Icons name="sliders" :size="12" />
        <span>Tunables</span>
      </button>
      <button class="tab-btn" :class="{ active: activeTab === 'logs' }" @click="switchTab('logs')">
        <Icons name="terminal" :size="12" />
        <span>Kill Monitor</span>
      </button>
    </div>

    <!-- Main Content -->
    <main class="content-area">
      <!-- ==================== DASHBOARD TAB ==================== -->
      <template v-if="activeTab === 'dashboard'">
        <!-- Master Pacifier Card -->
        <section class="md3-card">
          <div class="card-title-row">
            <span class="card-title">
              <Icons name="shield" :size="13" />
              <span>Background Kill Shield</span>
            </span>
            <span class="badge-pill" :class="status.stats.is_pacified ? 'success' : 'danger'">
              {{ status.stats.is_pacified ? 'Active Protection' : 'OEM Restrictions Active' }}
            </span>
          </div>

          <p style="font-size: 11.5px; color: var(--on-surface-variant); line-height: 1.4;">
            {{ status.stats.is_pacified
              ? 'HyperOS Scout, SPC, 5-minute memory killers, DuraSpeed, and Phantom limits are suppressed. Background apps can run uninterrupted.'
              : 'Aggressive killing mechanisms are active. OEM services may terminate apps within 5 minutes of screen-off.' }}
          </p>

          <div style="display: flex; gap: 8px;">
            <button
              class="btn btn-primary"
              style="flex: 1;"
              :disabled="isApplying"
              @click="pacifyAll"
            >
              <Icons name="zap" :size="13" />
              <span>{{ isApplying ? 'Pacifying...' : 'Pacify All Killers' }}</span>
            </button>
            <button
              class="btn btn-outline"
              :disabled="isApplying"
              @click="restoreStock"
              title="Revert to OEM stock behaviour"
            >
              <span>Restore Stock</span>
            </button>
          </div>
        </section>

        <!-- RAM & ZRAM Overview -->
        <section class="md3-card">
          <div class="card-title-row">
            <span class="card-title">
              <Icons name="cpu" :size="13" />
              <span>Memory & Cache</span>
            </span>
            <span class="badge-pill active">{{ formatMb(status.ram.used_mb) }} / {{ formatMb(status.ram.total_mb) }}</span>
          </div>

          <!-- Physical RAM Bar -->
          <div>
            <div style="display: flex; justify-content: space-between; font-size: 11px; margin-bottom: 4px;">
              <span style="color: var(--on-surface-variant);">Physical RAM</span>
              <span style="font-variant-numeric: tabular-nums;">{{ ramPercent }}% used ({{ formatMb(status.ram.free_mb) }} free)</span>
            </div>
            <div class="bar-track">
              <div class="bar-fill" :style="{ width: ramPercent + '%' }"></div>
            </div>
          </div>

          <!-- ZRAM / Swap Bar -->
          <div>
            <div style="display: flex; justify-content: space-between; font-size: 11px; margin-bottom: 4px;">
              <span style="color: var(--on-surface-variant);">ZRAM Compressed Swap</span>
              <span style="font-variant-numeric: tabular-nums;">{{ zramPercent }}% used ({{ formatMb(status.ram.zram_used_mb) }} / {{ formatMb(status.ram.zram_total_mb) }})</span>
            </div>
            <div class="bar-track">
              <div class="bar-fill" :style="{ width: zramPercent + '%', background: 'var(--secondary)' }"></div>
            </div>
          </div>
        </section>

        <!-- Subsystems Grid -->
        <section class="md3-card">
          <div class="card-title-row">
            <span class="card-title">
              <Icons name="layers" :size="13" />
              <span>Killer Subsystem Status</span>
            </span>
            <span class="badge-pill active">{{ status.stats.unrestricted_apps }} Unrestricted</span>
          </div>

          <div class="metrics-grid">
            <div class="metric-cell">
              <span class="metric-label">Phantom Process Limit</span>
              <span class="metric-val" :style="{ color: status.killers.phantom_limit === '2147483647' ? 'var(--success)' : 'var(--error)' }">
                {{ status.killers.phantom_limit === '2147483647' ? 'Unlimited' : status.killers.phantom_limit }}
              </span>
              <span class="metric-sub">Target: 2,147,483,647</span>
            </div>

            <div class="metric-cell">
              <span class="metric-label">Cached App Freezer</span>
              <span class="metric-val" :style="{ color: status.killers.cached_freezer === 'false' ? 'var(--success)' : 'var(--error)' }">
                {{ status.killers.cached_freezer === 'false' ? 'Disabled' : 'Active' }}
              </span>
              <span class="metric-sub">Android 14 cgroup v2</span>
            </div>

            <div class="metric-cell">
              <span class="metric-label">Xiaomi Scout Watchdog</span>
              <span class="metric-val" :style="{ color: !status.killers.scout_enable ? 'var(--success)' : 'var(--error)' }">
                {{ !status.killers.scout_enable ? 'Pacified' : 'Active' }}
              </span>
              <span class="metric-sub">Binder / memory kills</span>
            </div>

            <div class="metric-cell">
              <span class="metric-label">Smart Power Control</span>
              <span class="metric-val" :style="{ color: !status.killers.spc_enable ? 'var(--success)' : 'var(--error)' }">
                {{ !status.killers.spc_enable ? 'Pacified' : 'Active' }}
              </span>
              <span class="metric-sub">Max 2 background limit</span>
            </div>

            <div class="metric-cell">
              <span class="metric-label">Memory Standard (5m)</span>
              <span class="metric-val" :style="{ color: !status.killers.memory_standard_enable ? 'var(--success)' : 'var(--error)' }">
                {{ !status.killers.memory_standard_enable ? 'Disabled' : '300s Timer' }}
              </span>
              <span class="metric-sub">Xiaomi idle executioner</span>
            </div>

            <div class="metric-cell">
              <span class="metric-label">MTK DuraSpeed</span>
              <span class="metric-val" :style="{ color: status.killers.duraspeed_disabled ? 'var(--success)' : 'var(--error)' }">
                {{ status.killers.duraspeed_disabled ? 'Disabled' : 'Active' }}
              </span>
              <span class="metric-sub">MediaTek background killer</span>
            </div>

            <div class="metric-cell">
              <span class="metric-label">PowerKeeper Freezer</span>
              <span class="metric-val" :style="{ color: status.killers.pk_freezer === 'false' ? 'var(--success)' : 'var(--error)' }">
                {{ status.killers.pk_freezer === 'false' ? 'Disabled' : 'Active' }}
              </span>
              <span class="metric-sub">FrozenControlNewStatus</span>
            </div>

            <div class="metric-cell">
              <span class="metric-label">LMKD PSI Threshold</span>
              <span class="metric-val" :style="{ color: status.killers.lmkd_psi_partial !== '35' ? 'var(--success)' : 'var(--on-surface)' }">
                {{ status.killers.lmkd_psi_partial }} ms
              </span>
              <span class="metric-sub">Stock: 35 ms trigger</span>
            </div>
          </div>
        </section>
      </template>

      <!-- ==================== APPS MANAGER TAB ==================== -->
      <template v-else-if="activeTab === 'apps'">
        <section class="md3-card">
          <div class="card-title-row">
            <span class="card-title">
              <Icons name="apps" :size="13" />
              <span>Application Battery Rules</span>
            </span>
            <span class="badge-pill active">{{ filteredApps.length }} apps</span>
          </div>

          <!-- Quick Bulk Actions -->
          <div style="display: flex; gap: 8px;">
            <button class="btn btn-primary btn-sm" style="flex: 1;" @click="setAllUnrestricted">
              <Icons name="check" :size="12" />
              <span>Set All to No Restrictions</span>
            </button>
            <button class="btn btn-outline btn-sm" @click="whitelistUserApps">
              <Icons name="zap" :size="12" />
              <span>Whitelist User Apps</span>
            </button>
          </div>

          <!-- Search Box -->
          <div class="search-box">
            <Icons name="search" :size="13" style="color: var(--outline);" />
            <input
              type="text"
              v-model="searchQuery"
              placeholder="Search package name..."
            />
            <span v-if="searchQuery" @click="searchQuery = ''" style="cursor: pointer; color: var(--outline);">
              <Icons name="x" :size="12" />
            </span>
          </div>

          <!-- Filter Chips -->
          <div class="chips-row">
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
            <div
              class="chip"
              :class="{ active: appFilter === 'user' }"
              @click="appFilter = 'user'"
            >
              User Apps ({{ appsList.filter(a => a.isUser).length }})
            </div>
          </div>
        </section>

        <!-- Apps List -->
        <section class="md3-card">
          <div v-if="isLoadingApps" style="text-align: center; padding: 20px; color: var(--on-surface-variant);">
            <Icons name="refresh" :size="18" class="spin-anim" />
            <div style="margin-top: 8px; font-size: 12px;">Loading app policies...</div>
          </div>

          <div v-else-if="filteredApps.length === 0" style="text-align: center; padding: 24px; color: var(--on-surface-variant);">
            <div style="font-size: 12.5px;">No applications found</div>
            <div style="font-size: 11px; color: var(--outline); margin-top: 2px;">Try a different search query or filter</div>
          </div>

          <div v-else style="display: flex; flex-direction: column;">
            <div
              v-for="app in visibleApps"
              :key="app.pkg"
              class="list-row"
            >
              <div class="list-row-main">
                <div class="list-row-title">{{ app.pkg }}</div>
                <div style="display: flex; gap: 6px; margin-top: 4px; align-items: center;">
                  <span
                    class="badge-pill"
                    :class="app.bgControl === 'noRestrict' ? 'success' : 'danger'"
                    style="cursor: pointer;"
                    @click="toggleAppRestriction(app)"
                  >
                    {{ app.bgControl === 'noRestrict' ? 'No Restrictions' : 'miuiAuto (Killed)' }}
                  </span>
                  <span
                    class="badge-pill"
                    :class="app.whitelisted ? 'active' : ''"
                    style="cursor: pointer;"
                    @click="toggleAppWhitelist(app)"
                    title="Toggle Battery Optimization Whitelist"
                  >
                    {{ app.whitelisted ? 'Whitelisted' : 'Optimized' }}
                  </span>
                  <span v-if="app.isUser" class="badge-pill" style="color: var(--outline);">3rd-Party</span>
                </div>
              </div>

              <!-- Fast Toggle Switch -->
              <label class="switch">
                <input
                  type="checkbox"
                  :checked="app.bgControl === 'noRestrict'"
                  @change="toggleAppRestriction(app)"
                />
                <span class="slider"></span>
              </label>
            </div>

            <!-- Pagination limit warning -->
            <div v-if="filteredApps.length > visibleLimit" style="text-align: center; padding: 12px 0 4px 0;">
              <button class="btn btn-outline btn-sm btn-block" @click="visibleLimit += 50">
                Load More (Showing {{ visibleLimit }} of {{ filteredApps.length }})
              </button>
            </div>
          </div>
        </section>
      </template>

      <!-- ==================== TUNABLES TAB ==================== -->
      <template v-else-if="activeTab === 'tunables'">
        <section class="md3-card">
          <div class="card-title-row">
            <span class="card-title">
              <Icons name="sliders" :size="13" />
              <span>Killer Components Control</span>
            </span>
            <span class="badge-pill active">Modular Overrides</span>
          </div>

          <div style="display: flex; flex-direction: column;">
            <!-- Phantom Process Killer -->
            <div class="list-row">
              <div class="list-row-main">
                <div class="list-row-title">Android 14 Phantom Process Killer</div>
                <div class="list-row-desc">Stops ActivityManager from killing sub-processes (Termux tasks, workers) when exceeding 32 children.</div>
              </div>
              <label class="switch">
                <input
                  type="checkbox"
                  :checked="status.killers.phantom_limit === '2147483647'"
                  @change="toggleFeature('phantom', status.killers.phantom_limit !== '2147483647')"
                />
                <span class="slider"></span>
              </label>
            </div>

            <!-- Cached App Freezer -->
            <div class="list-row">
              <div class="list-row-main">
                <div class="list-row-title">Cached Apps Freezer</div>
                <div class="list-row-desc">Android 14 cgroup v2 task freezer. Disabling prevents broadcast stalls and background ANRs.</div>
              </div>
              <label class="switch">
                <input
                  type="checkbox"
                  :checked="status.killers.cached_freezer === 'false'"
                  @change="toggleFeature('freezer', status.killers.cached_freezer !== 'false')"
                />
                <span class="slider"></span>
              </label>
            </div>

            <!-- MediaTek DuraSpeed -->
            <div class="list-row">
              <div class="list-row-main">
                <div class="list-row-title">MediaTek DuraSpeed</div>
                <div class="list-row-desc">MediaTek OEM daemon that violently halts background processes to allocate RAM to foreground.</div>
              </div>
              <label class="switch">
                <input
                  type="checkbox"
                  :checked="status.killers.duraspeed_disabled"
                  @change="toggleFeature('duraspeed', !status.killers.duraspeed_disabled)"
                />
                <span class="slider"></span>
              </label>
            </div>

            <!-- Xiaomi Scout -->
            <div class="list-row">
              <div class="list-row-main">
                <div class="list-row-title">Xiaomi Scout Watchdog</div>
                <div class="list-row-desc">MIUI watchdog that kills applications upon binder memory spikes or high IPC load.</div>
              </div>
              <label class="switch">
                <input
                  type="checkbox"
                  :checked="!status.killers.scout_enable"
                  @change="toggleFeature('scout', status.killers.scout_enable)"
                />
                <span class="slider"></span>
              </label>
            </div>

            <!-- Smart Power Control -->
            <div class="list-row">
              <div class="list-row-main">
                <div class="list-row-title">Smart Power Control (SPC)</div>
                <div class="list-row-desc">Xiaomi power manager limiting protected background tasks to 2 processes.</div>
              </div>
              <label class="switch">
                <input
                  type="checkbox"
                  :checked="!status.killers.spc_enable"
                  @change="toggleFeature('spc', status.killers.spc_enable)"
                />
                <span class="slider"></span>
              </label>
            </div>

            <!-- Memory Standard 5m -->
            <div class="list-row">
              <div class="list-row-main">
                <div class="list-row-title">Memory Standard (300s Timer)</div>
                <div class="list-row-desc">Enforces a strict 5-minute background kill timer when the screen turns off.</div>
              </div>
              <label class="switch">
                <input
                  type="checkbox"
                  :checked="!status.killers.memory_standard_enable"
                  @change="toggleFeature('memstd', status.killers.memory_standard_enable)"
                />
                <span class="slider"></span>
              </label>
            </div>

            <!-- Camera Boost Killing -->
            <div class="list-row">
              <div class="list-row-main">
                <div class="list-row-title">Camera Boost Kill Adj (400:250:100:50)</div>
                <div class="list-row-desc">Opening the camera kills background apps down to adj 50. Relaxed to 999 to protect RAM.</div>
              </div>
              <label class="switch">
                <input
                  type="checkbox"
                  :checked="status.killers.camera_boost_adj.includes('999')"
                  @change="toggleFeature('camera', !status.killers.camera_boost_adj.includes('999'))"
                />
                <span class="slider"></span>
              </label>
            </div>

            <!-- LMKD PSI Threshold -->
            <div class="list-row">
              <div class="list-row-main">
                <div class="list-row-title">Relax LMKD PSI Stall (250ms vs 35ms)</div>
                <div class="list-row-desc">Prevents premature emergency thrashing kills on 8GB devices with generous headroom.</div>
              </div>
              <label class="switch">
                <input
                  type="checkbox"
                  :checked="status.killers.lmkd_psi_partial !== '35'"
                  @change="toggleFeature('lmkd', status.killers.lmkd_psi_partial === '35')"
                />
                <span class="slider"></span>
              </label>
            </div>

            <!-- PowerKeeper FrozenControl -->
            <div class="list-row">
              <div class="list-row-main">
                <div class="list-row-title">PowerKeeper FrozenApp Service</div>
                <div class="list-row-desc">Internal PowerKeeper thread freezing background processes. Disabling halts automatic sleep.</div>
              </div>
              <label class="switch">
                <input
                  type="checkbox"
                  :checked="status.killers.pk_freezer === 'false'"
                  @change="toggleFeature('pk_freezer', status.killers.pk_freezer !== 'false')"
                />
                <span class="slider"></span>
              </label>
            </div>
          </div>
        </section>
      </template>

      <!-- ==================== LOGS TAB ==================== -->
      <template v-else-if="activeTab === 'logs'">
        <section class="md3-card">
          <div class="card-title-row">
            <span class="card-title">
              <Icons name="terminal" :size="13" />
              <span>Real-Time Kill Activity Monitor</span>
            </span>
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

          <p style="font-size: 11px; color: var(--on-surface-variant);">
            Filters live system logcat for <code>lmkd</code>, <code>am_kill</code>, <code>lowmemory</code>, <code>scout</code>, and <code>powerkeeper</code> events.
          </p>

          <div class="console-box">
            <div v-if="isRefreshingLogs" style="color: var(--outline); text-align: center; padding: 20px;">
              Reading kill activity logs...
            </div>
            <div v-else-if="logsList.length === 0" style="color: var(--outline); text-align: center; padding: 20px;">
              No recent kill events detected in logcat buffer.
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

    <!-- Global Toast Feedback -->
    <div v-if="toastMsg" class="toast">
      {{ toastMsg }}
    </div>
  </div>
</template>

<script setup>
import { ref, computed, onMounted } from 'vue'
import Icons from './components/Icons.vue'
import { runBridgeJson } from './helpers/shell.js'

const activeTab = ref('dashboard')
const isRefreshing = ref(false)
const isApplying = ref(false)
const isLoadingApps = ref(false)
const isRefreshingLogs = ref(false)
const toastMsg = ref('')

const status = ref({
  device: { model: '', android: '', os: '', kernel: '' },
  ram: { total_mb: 0, used_mb: 0, free_mb: 0, zram_total_mb: 0, zram_used_mb: 0 },
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
    total_apps: 0,
    restricted_apps: 0,
    unrestricted_apps: 0,
    whitelisted_apps: 0,
    is_pacified: false
  }
})

const appsList = ref([])
const logsList = ref([])
const searchQuery = ref('')
const appFilter = ref('all')
const visibleLimit = ref(60)

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
    list = list.filter(a => a.pkg.toLowerCase().includes(q))
  }
  return list
})

const visibleApps = computed(() => {
  return filteredApps.value.slice(0, visibleLimit.value)
})

function showToast(msg) {
  toastMsg.value = msg
  setTimeout(() => {
    toastMsg.value = ''
  }, 2500)
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
  try {
    await runBridgeJson('pacify')
    showToast('All background killers pacified')
    await fetchStatus()
    if (appsList.value.length > 0) fetchApps()
  } catch (e) {
    showToast('Failed to pacify killers')
  } finally {
    isApplying.value = false
  }
}

async function restoreStock() {
  isApplying.value = true
  try {
    await runBridgeJson('restore_stock')
    showToast('OEM stock policies restored')
    await fetchStatus()
    if (appsList.value.length > 0) fetchApps()
  } catch (e) {
    showToast('Failed to restore stock policies')
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
  showToast('Updating all apps to no restrictions...')
  try {
    await runBridgeJson('set_all_unrestricted')
    appsList.value.forEach(a => a.bgControl = 'noRestrict')
    showToast('All apps set to No Restrictions')
    fetchStatus()
  } catch (e) {
    showToast('Error updating apps')
  }
}

async function whitelistUserApps() {
  showToast('Whitelisting user apps...')
  try {
    await runBridgeJson('pacify')
    appsList.value.forEach(a => {
      if (a.isUser) a.whitelisted = true
    })
    showToast('Third-party apps whitelisted')
    fetchStatus()
  } catch (e) {
    showToast('Failed to whitelist apps')
  }
}

async function toggleAppRestriction(app) {
  const nextMode = app.bgControl === 'noRestrict' ? 'miuiAuto' : 'noRestrict'
  app.bgControl = nextMode // Optimistic UI
  try {
    await runBridgeJson('set_app', app.pkg, nextMode)
    showToast(`${app.pkg.split('.').pop()}: ${nextMode === 'noRestrict' ? 'Unrestricted' : 'miuiAuto'}`)
  } catch (e) {
    app.bgControl = nextMode === 'noRestrict' ? 'miuiAuto' : 'noRestrict' // Revert
    showToast('Failed to update app policy')
  }
}

async function toggleAppWhitelist(app) {
  const nextVal = !app.whitelisted
  app.whitelisted = nextVal // Optimistic UI
  try {
    await runBridgeJson('whitelist_app', app.pkg, nextVal ? '1' : '0')
    showToast(`${app.pkg.split('.').pop()}: ${nextVal ? 'Whitelisted' : 'Optimized'}`)
  } catch (e) {
    app.whitelisted = !nextVal
    showToast('Failed to update whitelist')
  }
}

async function toggleFeature(key, enable) {
  try {
    await runBridgeJson('toggle', key, enable ? '1' : '0')
    showToast(`Updated ${key}`)
    fetchStatus()
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
