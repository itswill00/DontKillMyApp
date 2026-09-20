let cbSeq = 0

export function execCommand(cmd, timeoutMs = 6000) {
  return new Promise((resolve, reject) => {
    // 1. KernelSU Bridge (ksu or window.ksu)
    const ksuBridge = (typeof ksu !== 'undefined' && typeof ksu.exec === 'function')
      ? ksu
      : (typeof window !== 'undefined' && window.ksu && typeof window.ksu.exec === 'function' ? window.ksu : null)

    if (ksuBridge) {
      const id = `_dkma_${++cbSeq}_${Date.now()}`
      let settled = false

      const timer = setTimeout(() => {
        if (!settled) {
          settled = true
          if (window[id]) delete window[id]
          resolve('')
        }
      }, timeoutMs)

      window[id] = (errno, stdout, stderr) => {
        if (settled) return
        settled = true
        clearTimeout(timer)
        delete window[id]
        resolve(stdout || stderr || '')
      }

      try {
        ksuBridge.exec(cmd, '{}', id)
      } catch (e) {
        if (!settled) {
          settled = true
          clearTimeout(timer)
          delete window[id]
          resolve('')
        }
      }
      return
    }

    // 2. APatch / MMRL / Magisk WebRoot bridge
    const execBridge = (typeof exec === 'function')
      ? exec
      : (typeof window !== 'undefined' && typeof window.exec === 'function' ? window.exec : null)

    if (execBridge) {
      let settled = false
      const timer = setTimeout(() => {
        if (!settled) {
          settled = true
          resolve('')
        }
      }, timeoutMs)

      try {
        const res = execBridge(cmd, (errno, stdout, stderr) => {
          if (!settled) {
            settled = true
            clearTimeout(timer)
            resolve(stdout || stderr || '')
          }
        })
        if (res && typeof res.then === 'function') {
          res.then(r => {
            if (!settled) {
              settled = true
              clearTimeout(timer)
              resolve(typeof r === 'object' ? (r.stdout || r.stderr || '') : String(r))
            }
          }).catch(() => {
            if (!settled) {
              settled = true
              clearTimeout(timer)
              resolve('')
            }
          })
        }
      } catch (e) {
        if (!settled) {
          settled = true
          clearTimeout(timer)
          resolve('')
        }
      }
      return
    }

    // 3. Fallback mock for browser inspection
    resolve('')
  })
}

export async function runBridge(action, ...args) {
  const safeArgs = args.map(a => "'" + String(a).replace(/'/g, "'\\''") + "'").join(' ')
  const cmd = `/data/adb/modules/donykillmyapp/system/bin/libdonykillmyapp.so ${action} ${safeArgs}`
  try {
    const raw = await execCommand(cmd, 6000)
    return raw.trim()
  } catch (err) {
    console.error(`Bridge call failed for ${action}:`, err)
    return ''
  }
}

export async function runBridgeJson(action, ...args) {
  const raw = await runBridge(action, ...args)
  if (!raw) return null
  try {
    return JSON.parse(raw)
  } catch (e) {
    console.warn(`Failed to parse JSON for ${action}:`, raw)
    return null
  }
}

export function isKSU() {
  return (typeof ksu !== 'undefined' && typeof ksu.exec === 'function') ||
         (typeof window !== 'undefined' && window.ksu && typeof window.ksu.exec === 'function')
}

export function formatPkgName(pkg) {
  if (!pkg) return ''
  const parts = pkg.split('.').filter(Boolean)
  if (parts.length === 0) return pkg
  let last = parts[parts.length - 1]
  if (['android', 'app', 'client', 'game', 'gp'].includes(last.toLowerCase()) && parts.length > 1) {
    last = parts[parts.length - 2]
  }
  return last
    .replace(/([a-z])([A-Z])/g, '$1 $2')
    .replace(/[_\-.]+/g, ' ')
    .replace(/\b\w/g, c => c.toUpperCase())
    .trim() || pkg
}
