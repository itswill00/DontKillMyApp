let cbSeq = 0

export function execCommand(cmd, timeoutMs = 30000) {
  return new Promise((resolve, reject) => {
    if (typeof ksu !== 'undefined' && typeof ksu.exec === 'function') {
      const id = `_dkma_${++cbSeq}_${Date.now()}`

      const timer = setTimeout(() => {
        if (window[id]) {
          delete window[id]
          resolve('')
        }
      }, timeoutMs)

      window[id] = (errno, stdout, stderr) => {
        clearTimeout(timer)
        delete window[id]
        resolve(stdout || stderr || '')
      }

      try {
        ksu.exec(cmd, '{}', id)
      } catch (e) {
        clearTimeout(timer)
        delete window[id]
        reject(e)
      }
    } else if (typeof exec === 'function') {
      exec(cmd)
        .then(r => resolve(typeof r === 'object' ? (r.stdout || r.stderr || '') : String(r)))
        .catch(reject)
    } else {
      console.warn('Running in mock browser mode:', cmd)
      resolve('')
    }
  })
}

export async function runBridge(action, ...args) {
  const safeArgs = args.map(a => "'" + String(a).replace(/'/g, "'\\''") + "'").join(' ')
  const cmd = `for bin in /data/adb/modules/donykillmyapp/system/bin/libdonykillmyapp.so /data/data/com.termux/files/home/DontKillMyApp_Module/system/bin/libdonykillmyapp.so; do if [ -x "$bin" ]; then exec "$bin" ${action} ${safeArgs}; fi; done; echo '{"error":"bridge_not_found"}'`
  try {
    const raw = await execCommand(cmd, 25000)
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
