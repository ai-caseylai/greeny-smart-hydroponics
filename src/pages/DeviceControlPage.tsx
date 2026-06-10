import { useState, useEffect, useRef } from 'react'
import { useDevices } from '../hooks/useDevices'
import { useTelemetry } from '../hooks/useSensorData'
import { useOffice } from '../context/OfficeContext'
import { useWebSocket } from '../hooks/useWebSocket'
import { useTranslation } from 'react-i18next'
import { Search, Settings, Droplets, Zap, Thermometer, ChevronLeft, ChevronRight, Leaf, ToggleLeft, ToggleRight, Wifi, WifiOff, FlaskConical } from 'lucide-react'
import { timeAgo } from '../lib/utils'

const statusDotColors: Record<string, string> = {
  online: 'bg-green-500',
  offline: 'bg-gray-400',
  warning: 'bg-amber-500',
  maintenance: 'bg-blue-500',
  alarm: 'bg-red-500',
}

const statusBgColors: Record<string, string> = {
  online: 'border-green-200 bg-green-50/50',
  offline: 'border-gray-200 bg-gray-50/50',
  warning: 'border-amber-200 bg-amber-50/50',
  maintenance: 'border-blue-200 bg-blue-50/50',
  alarm: 'border-red-200 bg-red-50/50',
}

export default function DeviceControlPage() {
  const { selectedOfficeId } = useOffice()
  const { devices, loading } = useDevices(selectedOfficeId)
  const { data: telemetry } = useTelemetry(undefined, 200, selectedOfficeId)
  const { connected: wsConnected, on, sendRelay, sendPhCal } = useWebSocket(99)
  const { t } = useTranslation('devices')
  const [search, setSearch] = useState('')
  const [page, setPage] = useState(0)
  const [liveStatus, setLiveStatus] = useState<Record<string, string>>({})
  const pageSize = 20

  useEffect(() => {
    const unsub1 = on('device_status', (msg: any) => {
      setLiveStatus(prev => ({ ...prev, [msg.device_id]: msg.status }))
    })
    return unsub1
  }, [on])

  const statusLabels: Record<string, string> = {
    online: t('common:status.online'),
    offline: t('common:status.offline'),
    warning: t('common:status.warning'),
    maintenance: t('common:status.maintenance'),
    alarm: t('common:status.alarm'),
  }

  if (loading) return <div className="text-muted-foreground">{t('common:actions.loading')}</div>

  const filtered = devices.filter(d =>
    !search || d.name.toLowerCase().includes(search.toLowerCase()) || d.id.toLowerCase().includes(search.toLowerCase()) || d.location.toLowerCase().includes(search.toLowerCase())
  )
  const totalPages = Math.ceil(filtered.length / pageSize)
  const pageDevices = filtered.slice(page * pageSize, (page + 1) * pageSize)

  return (
    <div className="space-y-6">
      <div className="flex items-center justify-between">
        <h2 className="text-xl font-bold">{t('title')}</h2>
        <div className="flex items-center gap-3">
          <span className={`flex items-center gap-1 text-xs ${wsConnected ? 'text-green-600' : 'text-gray-400'}`}>
            {wsConnected ? <Wifi className="h-3 w-3" /> : <WifiOff className="h-3 w-3" />}
            {wsConnected ? 'Live' : 'Polling'}
          </span>
          <div className="relative">
            <Search className="absolute left-2.5 top-2.5 h-3.5 w-3.5 text-gray-400" />
            <input
              value={search}
              onChange={(e) => { setSearch(e.target.value); setPage(0) }}
              placeholder={t('search', { defaultValue: 'Search devices...' })}
              className="rounded-lg border border-border pl-8 pr-3 py-1.5 text-sm outline-none focus:border-[#00a65a] w-56"
            />
          </div>
          <span className="flex items-center gap-1.5 text-sm text-gray-500">
            <span className="h-2 w-2 rounded-full bg-green-500" />
            {t('common:status.online')} {devices.filter((d) => d.status === 'online').length}
          </span>
          <span className="flex items-center gap-1.5 text-sm text-gray-500">
            <span className="h-2 w-2 rounded-full bg-gray-400" />
            {t('common:status.offline')} {devices.filter((d) => d.status === 'offline').length}
          </span>
        </div>
      </div>

      <div className="grid grid-cols-1 gap-4 sm:grid-cols-2 lg:grid-cols-3 xl:grid-cols-4">
        {pageDevices.map((d) => {
          const latestT = telemetry.find((t) => t.device_id === d.id)
          const effectiveStatus = liveStatus[d.id] || d.status
          return (
            <DeviceCard key={d.id} device={{ ...d, status: effectiveStatus as any }} latestT={latestT} statusLabels={statusLabels} t={t} sendRelay={sendRelay} sendPhCal={sendPhCal} />
          )
        })}
      </div>

      {totalPages > 1 && (
        <div className="flex items-center justify-between">
          <span className="text-xs text-gray-400">
            {filtered.length} {t('devices', { defaultValue: 'devices' })} — {t('pageOf', { defaultValue: `Page ${page + 1} of ${totalPages}` })}
          </span>
          <div className="flex gap-1">
            <button onClick={() => setPage(Math.max(0, page - 1))} disabled={page === 0}
              className="rounded-lg border border-border p-1.5 disabled:opacity-30 hover:bg-gray-50">
              <ChevronLeft className="h-4 w-4" />
            </button>
            {Array.from({ length: Math.min(5, totalPages) }, (_, i) => {
              const start = Math.max(0, Math.min(page - 2, totalPages - 5))
              const pageNum = start + i
              if (pageNum >= totalPages) return null
              return (
                <button key={pageNum} onClick={() => setPage(pageNum)}
                  className={`rounded-lg border px-3 py-1 text-xs ${pageNum === page ? 'border-[#00a65a] bg-[#e8f5e9] text-[#2E7D32]' : 'border-border hover:bg-gray-50'}`}>
                  {pageNum + 1}
                </button>
              )
            })}
            <button onClick={() => setPage(Math.min(totalPages - 1, page + 1))} disabled={page >= totalPages - 1}
              className="rounded-lg border border-border p-1.5 disabled:opacity-30 hover:bg-gray-50">
              <ChevronRight className="h-4 w-4" />
            </button>
          </div>
        </div>
      )}
    </div>
  )
}

function DeviceCard({ device: d, latestT, statusLabels, t, sendRelay, sendPhCal }: {
  device: any; latestT: any; statusLabels: Record<string, string>; t: (key: string, opts?: any) => string
  sendRelay: (id: string, r1: number, r2: number, r3?: number, r4?: number) => void
  sendPhCal: (id: string, cal: number) => void
}) {
  const [relay1, setRelay1] = useState(latestT?.relay1 === 1)
  const [relay2, setRelay2] = useState(latestT?.relay2 === 1)
  useEffect(() => { setRelay1(latestT?.relay1 === 1) }, [latestT?.relay1])
  useEffect(() => { setRelay2(latestT?.relay2 === 1) }, [latestT?.relay2])
  const [phCal, setPhCal] = useState(latestT?.ph_cal != null ? String(latestT.ph_cal) : '18.07')
  const [showPhCal, setShowPhCal] = useState(false)

  const toggleRelay = (num: number, value: boolean) => {
    if (num === 1) {
      setRelay1(value)
      sendRelay(d.id, value ? 1 : 0, relay2 ? 1 : 0)
    } else {
      setRelay2(value)
      sendRelay(d.id, relay1 ? 1 : 0, value ? 1 : 0)
    }
  }

  const applyPhCal = () => {
    const val = parseFloat(phCal)
    if (!isNaN(val) && val > 0) sendPhCal(d.id, val)
  }

  const [relayStatus, setRelayStatus] = useState<'idle' | 'waiting' | 'ok'>('idle')
  const sentRelayRef = useRef<{ num: number; val: number } | null>(null)

  // 監測 telemetry 確認 relay 已執行
  useEffect(() => {
    const sent = sentRelayRef.current
    if (!sent || !latestT) return
    const actual = sent.num === 1 ? latestT.relay1 : latestT.relay2
    if (actual === sent.val) {
      setRelayStatus('ok')
      sentRelayRef.current = null
      const now = new Date().toLocaleTimeString()
      const ackTime = new Date().toLocaleTimeString()
      setCmdLog(prev => {
        const updated = [...prev]
        if (updated[0]) updated[0] = { ...updated[0], status: '✓ 執行成功', ackTime }
        return updated
      })
      setTimeout(() => setRelayStatus('idle'), 2000)
    }
  }, [latestT?.relay1, latestT?.relay2, latestT])

  const [cmdLog, setCmdLog] = useState<{ time: string; action: string; status: string; ackTime?: string }[]>([])
  const [showLog, setShowLog] = useState(false)

  // 載入 SQL 中的命令日誌
  useEffect(() => {
    const loadLogs = async () => {
      try {
        const token = localStorage.getItem('token')
        const apiBase = (import.meta as any).env?.VITE_API_BASE || '/api'
        const res = await fetch(`${apiBase}/relay-logs?device_id=${d.id}&limit=20`, {
          headers: { Authorization: `Bearer ${token}` }
        })
        if (res.ok) {
          const data = await res.json()
          setCmdLog(data.map((r: any) => ({
            time: new Date(r.created_at * 1000).toLocaleTimeString(),
            action: r.ph_cal != null ? `pH Cal=${r.ph_cal}` : `R1=${r.relay1 ?? '-'} R2=${r.relay2 ?? '-'}`,
            status: r.status === 'ack' ? '✓ 已執行' : '已發送',
            ackTime: r.status === 'ack' ? new Date(r.created_at * 1000).toLocaleTimeString() : undefined,
          })))
        }
      } catch {}
    }
    loadLogs()
  }, [d.id, latestT?.created_at])

  const toggleWithStatus = (num: number, value: boolean) => {
    const v = value ? 1 : 0
    const action = `R${num}=${v ? 'ON' : 'OFF'}`
    const now = new Date().toLocaleTimeString()
    if (num === 1) { setRelay1(value); sendRelay(d.id, v, relay2 ? 1 : 0) }
    else { setRelay2(value); sendRelay(d.id, relay1 ? 1 : 0, v) }
    sentRelayRef.current = { num, val: v }
    setRelayStatus('waiting')
    setCmdLog(prev => [{ time: now, action, status: '已發送' }, ...prev].slice(0, 20))
  }

  const statusLabel: Record<string, { text: string; color: string }> = {
    idle: { text: '等待命令', color: 'text-gray-400' },
    waiting: { text: '等待回應...', color: 'text-blue-500' },
    ok: { text: '執行成功', color: 'text-green-500' },
  }

  return (
    <div className={`rounded-xl border p-5 shadow-sm transition-all hover:shadow-md ${statusBgColors[d.status] || 'border-border bg-white'}`}>
      <div className="flex items-center justify-between mb-4">
        <div className="flex items-center gap-2">
          <span className={`h-2.5 w-2.5 rounded-full ${statusDotColors[d.status] || 'bg-gray-400'}`} />
          <div>
            <p className="font-semibold text-gray-900">{d.name}</p>
            <p className="text-xs text-gray-400">{d.location} · {d.id}</p>
          </div>
        </div>
        <span className={`rounded-full px-2 py-0.5 text-xs font-medium ${
          d.status === 'online' ? 'bg-green-100 text-green-700' :
          d.status === 'warning' || d.status === 'alarm' ? 'bg-amber-100 text-amber-700' :
          d.status === 'maintenance' ? 'bg-blue-100 text-blue-700' :
          'bg-gray-100 text-gray-700'
        }`}>
          {statusLabels[d.status] || d.status}
        </span>
      </div>

      {latestT && (
        <div className="grid grid-cols-2 gap-3 mb-4">
          <div className="flex items-center gap-2">
            <Droplets className="h-3.5 w-3.5 text-blue-500" />
            <div>
              <p className="text-[10px] text-gray-400">pH</p>
              <p className="text-sm font-medium">{latestT.ph?.toFixed(2) ?? '-'}</p>
            </div>
          </div>
          <div className="flex items-center gap-2">
            <Zap className="h-3.5 w-3.5 text-green-500" />
            <div>
              <p className="text-[10px] text-gray-400">TDS</p>
              <p className="text-sm font-medium">{latestT.tds != null ? Math.round(latestT.tds) : (latestT.ec != null ? Math.round(latestT.ec) : '-')} ppm</p>
            </div>
          </div>
          <div className="flex items-center gap-2">
            <Thermometer className="h-3.5 w-3.5 text-pink-500" />
            <div>
              <p className="text-[10px] text-gray-400">Temp</p>
              <p className="text-sm font-medium">{latestT.water_temp?.toFixed(1) ?? '-'}°C</p>
            </div>
          </div>
        </div>
      )}

      {/* Relay controls */}
      <div className="flex items-center gap-3 pt-3 border-t border-border/50 mb-2">
        {[1, 2].map(n => {
          const states = [relay1, relay2]
          return (
            <button key={n} onClick={() => toggleWithStatus(n, !states[n - 1])} className="flex items-center gap-1 text-xs">
              {states[n - 1] ? <ToggleRight className="h-4 w-4 text-[#00a65a]" /> : <ToggleLeft className="h-4 w-4 text-gray-400" />}
              <span className={states[n - 1] ? 'text-[#00a65a] font-medium' : 'text-gray-500'}>R{n}</span>
            </button>
          )
        })}
        <span className={`text-[10px] ml-auto ${statusLabel[relayStatus].color}`}>{statusLabel[relayStatus].text}</span>
      </div>

      {/* Command log toggle */}
      <div className="pt-2 border-t border-border/50 mb-2">
        <button onClick={() => setShowLog(!showLog)} className="flex items-center gap-1 text-[10px] text-gray-400 hover:text-gray-600">
          📋 命令日誌 {cmdLog.length > 0 && `(${cmdLog.length})`}
        </button>
        {showLog && cmdLog.length > 0 && (
          <div className="mt-1 max-h-24 overflow-y-auto space-y-0.5">
            {cmdLog.slice(0, 10).map((entry, i) => (
              <div key={i} className="flex items-center gap-2 text-[10px]">
                <span className="text-gray-400 w-10">{entry.time}</span>
                <span className="text-gray-600">{entry.action}</span>
                <span className={entry.status.includes('✓') ? 'text-green-500' : 'text-blue-400'}>
                  {entry.status}
                </span>
                {entry.ackTime && (
                  <span className="text-gray-400">ACK {entry.ackTime}</span>
                )}
              </div>
            ))}
          </div>
        )}
        {showLog && cmdLog.length === 0 && (
          <p className="text-[10px] text-gray-400 mt-1">暫無命令記錄</p>
        )}
      </div>

      {/* pH Calibration */}
      <div className="pt-2 border-t border-border/50 mb-2">
        <button onClick={() => setShowPhCal(!showPhCal)} className="flex items-center gap-1 text-xs text-gray-400 hover:text-[#00a65a]">
          <FlaskConical className="h-3 w-3" />
          pH Cal
        </button>
        {showPhCal && (
          <div className="flex items-center gap-2 mt-2">
            <input
              type="number" step="0.01" value={phCal}
              onChange={(e) => setPhCal(e.target.value)}
              className="w-20 rounded border border-border px-2 py-1 text-xs outline-none focus:border-[#00a65a]"
            />
            <button onClick={applyPhCal}
              className="rounded bg-[#00a65a] px-2 py-1 text-xs text-white hover:bg-[#008a4a]">
              Set
            </button>
          </div>
        )}
      </div>

      <div className="flex items-center justify-between pt-3 border-t border-border/50">
        <span className="text-[10px] text-gray-400">{d.last_seen ? timeAgo(d.last_seen) : t('notConnected')}</span>
        <div className="flex gap-1.5">
          <button className="rounded-lg p-1.5 hover:bg-white/80 transition-colors" title={t('settings')}>
            <Settings className="h-3.5 w-3.5 text-gray-400" />
          </button>
        </div>
      </div>
    </div>
  )
}
