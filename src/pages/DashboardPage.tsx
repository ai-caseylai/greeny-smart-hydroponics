import { useState, useEffect } from 'react'
import { apiFetch } from '../lib/api'
import { useDevices } from '../hooks/useDevices'
import { useTelemetry } from '../hooks/useSensorData'
import { useOffice } from '../context/OfficeContext'
import { useRacks } from '../hooks/useRacks'
import { useTranslation } from 'react-i18next'
import {
  LineChart, Line, XAxis, YAxis, CartesianGrid, Tooltip, ResponsiveContainer,
  PieChart, Pie, Cell,
} from 'recharts'
import { Cpu, AlertTriangle, Droplets, Thermometer, RefreshCw, Leaf } from 'lucide-react'
import { timeAgo } from '../lib/utils'
import type { DashboardStats } from '../types'

const pieColors = ['#4CAF50', '#9E9E9E', '#FF9800', '#2196F3', '#E91E63']

export default function DashboardPage() {
  const { selectedOfficeId } = useOffice()
  const [stats, setStats] = useState<DashboardStats | null>(null)
  const [loading, setLoading] = useState(true)
  const { devices } = useDevices(selectedOfficeId)
  const { data: telemetry } = useTelemetry(undefined, 200, selectedOfficeId)
  const { racks } = useRacks(selectedOfficeId ?? undefined)
  const [selectedRackId, setSelectedRackId] = useState<number | null>(null)
  const { t } = useTranslation(['dashboard', 'common'])

  // Build office name map from racks
  const deviceOfficeMap = new Map<string, string>()
  racks.forEach(r => {
    if (r.device_id && r.office_name) {
      if (!deviceOfficeMap.has(r.device_id)) deviceOfficeMap.set(r.device_id, r.office_name)
    }
  })

  const rackDeviceIds = selectedRackId
    ? racks.filter(r => r.id === selectedRackId && r.device_id).map(r => r.device_id)
    : null
  const filteredDevices = rackDeviceIds
    ? devices.filter(d => rackDeviceIds.includes(d.id))
    : devices
  const filteredTelemetry = rackDeviceIds
    ? telemetry.filter(t => rackDeviceIds.includes(t.device_id))
    : telemetry

  const fetchStats = async () => {
    try {
      const params = selectedOfficeId ? `?office_id=${selectedOfficeId}` : ''
      const data = await apiFetch<DashboardStats>(`/dashboard/stats${params}`)
      setStats(data)
    } catch { /* */ } finally {
      setLoading(false)
    }
  }

  useEffect(() => { fetchStats() }, [selectedOfficeId])

  if (loading) return <div className="text-muted-foreground">{t('common:actions.loading')}</div>

  const statusLabels: Record<string, string> = {
    online: t('common:status.online'), offline: t('common:status.offline'),
    warning: t('common:status.warning'), alarm: t('common:status.alarm'),
    maintenance: t('common:status.maintenance'),
  }
  const statusColors: Record<string, string> = {
    online: 'bg-green-500', offline: 'bg-gray-400', warning: 'bg-amber-500',
    maintenance: 'bg-blue-500', alarm: 'bg-red-500',
  }

  const kpiCards = [
    { label: t('onlineDevices'), value: `${stats?.online_devices || 0}/${stats?.total_devices || 0}`, icon: Cpu, color: '#4CAF50' },
    { label: t('todayAlerts'), value: stats?.today_alerts || 0, icon: AlertTriangle, color: '#FF9800' },
    { label: t('avgPH'), value: stats?.avg_ph?.toFixed(1) || '-', icon: Droplets, color: '#2196F3' },
    { label: t('avgTemp'), value: stats?.avg_temp ? `${stats.avg_temp.toFixed(0)}°C` : '-', icon: Thermometer, color: '#E91E63' },
    { label: 'NDVI', value: stats?.avg_ndvi?.toFixed(2) || '-', icon: Leaf, color: '#66BB6A' },
  ]

  const trendMap = new Map<string, { ph: number[]; temp: number[] }>()
  filteredTelemetry.forEach((t) => {
    const day = new Date(t.ts_ms).toLocaleDateString()
    if (!trendMap.has(day)) trendMap.set(day, { ph: [], temp: [] })
    trendMap.get(day)!.ph.push(t.ph)
    trendMap.get(day)!.temp.push(t.water_temp)
  })
  const trendData = Array.from(trendMap.entries()).map(([date, vals]) => ({
    date, pH: Math.round((vals.ph.reduce((a, b) => a + b, 0) / vals.ph.length) * 10) / 10,
    Temp: Math.round((vals.temp.reduce((a, b) => a + b, 0) / vals.temp.length) * 10) / 10,
  }))

  const distData = stats?.device_distribution?.map((d) => ({
    name: statusLabels[d.status] || d.status, value: d.count,
  })) || []

  return (
    <div className="space-y-6">
      <div className="flex items-center justify-between">
        <h2 className="text-xl font-bold">{t('title')}</h2>
        <div className="flex items-center gap-3">
          {racks.length > 0 && (
            <select
              value={selectedRackId ?? ''}
              onChange={(e) => setSelectedRackId(e.target.value ? Number(e.target.value) : null)}
              className="rounded-lg border border-border px-3 py-1.5 text-sm"
            >
              <option value="">{t('racks:allRacks', { defaultValue: 'All Racks' })}</option>
              {racks.map(r => <option key={r.id} value={r.id}>{r.name}</option>)}
            </select>
          )}
          <button onClick={fetchStats} className="flex items-center gap-1.5 rounded-lg border border-border px-3 py-1.5 text-sm hover:bg-gray-50">
            <RefreshCw className="h-3.5 w-3.5" />{t('common:actions.refresh')}
          </button>
        </div>
      </div>

      <div className="grid grid-cols-1 gap-4 sm:grid-cols-2 lg:grid-cols-5">
        {kpiCards.map((card) => (
          <div key={card.label} className="rounded-xl border border-border bg-white p-5 shadow-sm">
            <div className="flex items-center justify-between">
              <div><p className="text-sm text-gray-500">{card.label}</p><p className="mt-1 text-2xl font-bold text-gray-900">{card.value}</p></div>
              <div className="flex h-10 w-10 items-center justify-center rounded-lg" style={{ backgroundColor: `${card.color}15` }}>
                <card.icon className="h-5 w-5" style={{ color: card.color }} />
              </div>
            </div>
          </div>
        ))}
      </div>

      <div className="grid grid-cols-1 gap-6 lg:grid-cols-3">
        <div className="lg:col-span-2 rounded-xl border border-border bg-white p-5 shadow-sm">
          <h3 className="mb-4 text-sm font-semibold text-gray-700">{t('trend7d')}</h3>
          <ResponsiveContainer width="100%" height={280}>
            <LineChart data={trendData}>
              <CartesianGrid strokeDasharray="3 3" stroke="#f0f0f0" />
              <XAxis dataKey="date" tick={{ fontSize: 12 }} />
              <YAxis tick={{ fontSize: 12 }} />
              <Tooltip />
              <Line type="monotone" dataKey="pH" stroke="#2196F3" strokeWidth={2} dot={{ r: 4 }} name="pH" />
              <Line type="monotone" dataKey="Temp" stroke="#E91E63" strokeWidth={2} dot={{ r: 4 }} name="°C" />
            </LineChart>
          </ResponsiveContainer>
        </div>

        <div className="rounded-xl border border-border bg-white p-5 shadow-sm">
          <h3 className="mb-4 text-sm font-semibold text-gray-700">{t('deviceDist')}</h3>
          <ResponsiveContainer width="100%" height={280}>
            <PieChart>
              <Pie data={distData} cx="50%" cy="50%" innerRadius={60} outerRadius={90} paddingAngle={3} dataKey="value"
                label={({ name, value }) => `${name} ${value}`}>
                {distData.map((_, i) => <Cell key={i} fill={pieColors[i % pieColors.length]} />)}
              </Pie>
              <Tooltip />
            </PieChart>
          </ResponsiveContainer>
        </div>
      </div>

      <div className="rounded-xl border border-border bg-white shadow-sm overflow-hidden">
        <div className="px-5 py-4 border-b border-border"><h3 className="text-sm font-semibold text-gray-700">{t('deviceStatus')}</h3></div>
        <div className="overflow-x-auto">
          <table className="w-full text-sm">
            <thead>
              <tr className="border-b border-border bg-gray-50 text-left text-gray-500">
                <th className="px-4 py-3">{t('deviceId')}</th>
                <th className="px-4 py-3">{t('deviceName')}</th>
                <th className="px-4 py-3">{t('company', { defaultValue: 'Company' })}</th>
                <th className="px-4 py-3">{t('location')}</th>
                <th className="px-4 py-3">pH</th>
                <th className="px-4 py-3">EC (μS/cm)</th>
                <th className="px-4 py-3">°C</th>
                <th className="px-4 py-3">NDVI</th>
                <th className="px-4 py-3">{t('common:status.online', { defaultValue: 'Status' })}</th>
                <th className="px-4 py-3">{t('lastReport')}</th>
              </tr>
            </thead>
            <tbody>
              {filteredDevices.map((d) => {
                const lt = filteredTelemetry.find((t) => t.device_id === d.id)
                const officeName = deviceOfficeMap.get(d.id) || (d as any).office_name || '-'
                return (
                  <tr key={d.id} className="border-b border-border/50 hover:bg-gray-50/50">
                    <td className="px-4 py-3 font-mono text-xs text-gray-600">{d.id}</td>
                    <td className="px-4 py-3 font-medium text-gray-900">{d.name}</td>
                    <td className="px-4 py-3 text-gray-600">{officeName}</td>
                    <td className="px-4 py-3 text-gray-600">{d.location}</td>
                    <td className="px-4 py-3">{lt?.ph?.toFixed(1) || '-'}</td>
                    <td className="px-4 py-3">{lt?.ec || '-'}</td>
                    <td className="px-4 py-3">{lt?.water_temp?.toFixed(1) || '-'}</td>
                    <td className="px-4 py-3">{lt?.ndvi?.toFixed(2) || '-'}</td>
                    <td className="px-4 py-3"><span className="flex items-center gap-1.5"><span className={`h-2 w-2 rounded-full ${statusColors[d.status] || 'bg-gray-400'}`} />{statusLabels[d.status] || d.status}</span></td>
                    <td className="px-4 py-3 text-xs text-gray-400">{d.last_seen ? timeAgo(d.last_seen) : '-'}</td>
                  </tr>
                )
              })}
            </tbody>
          </table>
        </div>
      </div>
    </div>
  )
}
