import { useState } from 'react'
import { useDevices } from '../hooks/useDevices'
import { useTelemetry } from '../hooks/useSensorData'
import { useOffice } from '../context/OfficeContext'
import { useRacks } from '../hooks/useRacks'
import { useTranslation } from 'react-i18next'
import {
  LineChart, Line, XAxis, YAxis, CartesianGrid, Tooltip, ResponsiveContainer,
  BarChart, Bar, Legend,
} from 'recharts'
import { Droplets, Thermometer, Zap, Leaf, RefreshCw, ToggleLeft, ToggleRight } from 'lucide-react'
import { formatTime } from '../lib/utils'

function GaugeCard({ label, value, unit, icon: Icon, color, min, max, optimal }: {
  label: string; value: number | null; unit: string; icon: React.ElementType
  color: string; min: number; max: number; optimal: [number, number]
}) {
  const { t } = useTranslation('common')
  const pct = value !== null ? Math.min(100, Math.max(0, ((value - min) / (max - min)) * 100)) : 0
  const inRange = value !== null && value >= optimal[0] && value <= optimal[1]
  return (
    <div className="rounded-lg border border-gray-100 bg-gray-50 p-3">
      <div className="flex items-center gap-1.5 mb-2">
        <Icon className="h-3.5 w-3.5" style={{ color }} />
        <span className="text-xs text-gray-500">{label}</span>
      </div>
      <div className="flex items-baseline gap-0.5 mb-2">
        <span className="text-xl font-bold text-gray-900">{value !== null ? (Number.isInteger(value) ? value : value.toFixed(1)) : '-'}</span>
        <span className="text-[10px] text-gray-400">{unit}</span>
      </div>
      <div className="h-1.5 rounded-full bg-gray-200 overflow-hidden">
        <div className="h-full rounded-full transition-all duration-500" style={{ width: `${pct}%`, backgroundColor: inRange ? color : '#FF9800' }} />
      </div>
      <div className="mt-1 flex justify-between text-[9px] text-gray-400">
        <span>{min}</span>
        <span className={inRange ? 'text-green-600' : 'text-amber-600'}>{inRange ? t('status.normal') : t('status.abnormal')}</span>
        <span>{max}</span>
      </div>
    </div>
  )
}

function RackCard({ rackName, deviceName, telemetry, spectralData }: {
  rackName: string; deviceName: string; telemetry: any[]; spectralData: any[]
}) {
  const { t } = useTranslation(['water', 'common'])
  const latest = telemetry.length > 0 ? telemetry[0] : null

  const chartData = [...telemetry].reverse().map((r) => ({
    time: new Date(r.ts_ms).toLocaleTimeString(undefined, { hour: '2-digit', minute: '2-digit' }),
    pH: r.ph, EC: r.ec, Temp: r.water_temp, NDVI: r.ndvi,
  }))

  const [page, setPage] = useState(0)
  const pageSize = 5
  const pageData = telemetry.slice(page * pageSize, (page + 1) * pageSize)
  const totalPages = Math.ceil(telemetry.length / pageSize)

  // Relay toggle state (local UI only)
  const [relay1, setRelay1] = useState(latest?.relay1 === 1)
  const [relay2, setRelay2] = useState(latest?.relay2 === 1)

  return (
    <div className="rounded-xl border border-border bg-white shadow-sm overflow-hidden">
      {/* Rack header */}
      <div className="flex items-center justify-between px-5 py-3 bg-gradient-to-r from-green-50 to-white border-b border-border">
        <div className="flex items-center gap-2">
          <Leaf className="h-4 w-4 text-[#2E7D32]" />
          <h3 className="text-sm font-semibold text-gray-800">{rackName}</h3>
          {deviceName && <span className="text-[10px] text-gray-400">({deviceName})</span>}
        </div>
        {latest && (
          <span className="text-[10px] text-gray-400">
            {t('common:actions.lastUpdated', { defaultValue: 'Last updated' })}: {formatTime(latest.created_at)}
          </span>
        )}
      </div>

      {/* Gauge cards */}
      <div className="grid grid-cols-1 gap-3 p-4 sm:grid-cols-2 lg:grid-cols-4">
        <GaugeCard label={t('ph')} value={latest?.ph ?? null} unit="" icon={Droplets} color="#2196F3" min={0} max={14} optimal={[5.5, 7.0]} />
        <GaugeCard label={t('ec')} value={latest?.ec ?? null} unit={t('unitEC')} icon={Zap} color="#4CAF50" min={0} max={3000} optimal={[800, 1500]} />
        <GaugeCard label={t('waterTemp')} value={latest?.water_temp ?? null} unit={t('unitTemp')} icon={Thermometer} color="#E91E63" min={0} max={50} optimal={[18, 28]} />
        <GaugeCard label="NDVI" value={latest?.ndvi ?? null} unit="" icon={Leaf} color="#66BB6A" min={-1} max={1} optimal={[0.3, 0.8]} />
      </div>

      {/* Spectrum chart */}
      {spectralData.length > 0 && (
        <div className="px-5 pb-4">
          <h4 className="mb-2 text-xs font-semibold text-gray-600">{t('spectrum', { defaultValue: 'Multi-Spectrum' })}</h4>
          <ResponsiveContainer width="100%" height={180}>
            <BarChart data={spectralData}>
              <CartesianGrid strokeDasharray="3 3" stroke="#f0f0f0" />
              <XAxis dataKey="name" tick={{ fontSize: 10 }} />
              <YAxis tick={{ fontSize: 10 }} />
              <Tooltip />
              <Bar dataKey="intensity" fill="#4CAF50" radius={[4, 4, 0, 0]} name={t('intensity', { defaultValue: 'Intensity' })} />
            </BarChart>
          </ResponsiveContainer>
        </div>
      )}

      {/* Trend chart */}
      {chartData.length > 0 && (
        <div className="px-5 pb-4">
          <h4 className="mb-2 text-xs font-semibold text-gray-600">{t('trend24h')}</h4>
          <ResponsiveContainer width="100%" height={200}>
            <LineChart data={chartData}>
              <CartesianGrid strokeDasharray="3 3" stroke="#f0f0f0" />
              <XAxis dataKey="time" tick={{ fontSize: 10 }} />
              <YAxis tick={{ fontSize: 10 }} />
              <Tooltip />
              <Legend wrapperStyle={{ fontSize: 10 }} />
              <Line type="monotone" dataKey="pH" stroke="#2196F3" strokeWidth={1.5} dot={false} name="pH" />
              <Line type="monotone" dataKey="Temp" stroke="#E91E63" strokeWidth={1.5} dot={false} name="°C" />
              <Line type="monotone" dataKey="EC" stroke="#4CAF50" strokeWidth={1.5} dot={false} name="EC" />
            </LineChart>
          </ResponsiveContainer>
        </div>
      )}

      {/* Relay controls */}
      <div className="px-5 py-3 border-t border-border bg-gray-50/50">
        <div className="flex items-center gap-4">
          <span className="text-xs font-medium text-gray-600">{t('relays', { defaultValue: 'Relays' })}</span>
          <button onClick={() => setRelay1(!relay1)} className="flex items-center gap-1.5 text-xs">
            {relay1 ? <ToggleRight className="h-5 w-5 text-[#00a65a]" /> : <ToggleLeft className="h-5 w-5 text-gray-400" />}
            <span className={relay1 ? 'text-[#00a65a]' : 'text-gray-500'}>Relay 1</span>
          </button>
          <button onClick={() => setRelay2(!relay2)} className="flex items-center gap-1.5 text-xs">
            {relay2 ? <ToggleRight className="h-5 w-5 text-[#00a65a]" /> : <ToggleLeft className="h-5 w-5 text-gray-400" />}
            <span className={relay2 ? 'text-[#00a65a]' : 'text-gray-500'}>Relay 2</span>
          </button>
        </div>
      </div>

      {/* Data table */}
      <div className="border-t border-border">
        <table className="w-full text-xs">
          <thead>
            <tr className="border-b border-border bg-gray-50 text-left text-gray-500">
              <th className="px-4 py-2">pH</th>
              <th className="px-4 py-2">EC</th>
              <th className="px-4 py-2">°C</th>
              <th className="px-4 py-2">NDVI</th>
              <th className="px-4 py-2">%</th>
              <th className="px-4 py-2">{t('time')}</th>
            </tr>
          </thead>
          <tbody>
            {pageData.map((r) => (
              <tr key={r.id} className="border-b border-border/30 hover:bg-gray-50/50">
                <td className="px-4 py-2">{r.ph.toFixed(1)}</td>
                <td className="px-4 py-2">{r.ec}</td>
                <td className="px-4 py-2">{r.water_temp.toFixed(1)}</td>
                <td className="px-4 py-2">{r.ndvi.toFixed(2)}</td>
                <td className="px-4 py-2">{r.water_level}%</td>
                <td className="px-4 py-2 text-gray-400">{formatTime(r.created_at)}</td>
              </tr>
            ))}
          </tbody>
        </table>
        {totalPages > 1 && (
          <div className="flex items-center justify-between px-4 py-2 border-t border-border bg-gray-50/30">
            <span className="text-[10px] text-gray-400">{t('totalRecords', { count: telemetry.length })}</span>
            <div className="flex gap-1">
              <button onClick={() => setPage(Math.max(0, page - 1))} disabled={page === 0}
                className="rounded px-2 py-0.5 text-[10px] border border-border disabled:opacity-30 hover:bg-gray-50">{t('prev')}</button>
              <button onClick={() => setPage(Math.min(totalPages - 1, page + 1))} disabled={page >= totalPages - 1}
                className="rounded px-2 py-0.5 text-[10px] border border-border disabled:opacity-30 hover:bg-gray-50">{t('next')}</button>
            </div>
          </div>
        )}
      </div>
    </div>
  )
}

export default function WaterQualityPage() {
  const { selectedOfficeId } = useOffice()
  const { devices } = useDevices(selectedOfficeId)
  const { racks } = useRacks(selectedOfficeId ?? undefined)
  const { data, loading, refetch } = useTelemetry(undefined, 200, selectedOfficeId)
  const { t } = useTranslation(['water', 'common'])

  // Build per-rack data
  const racksWithData = racks
    .filter(r => r.device_id)
    .map(rack => {
      const rackTelemetry = data.filter(t => t.device_id === rack.device_id)
      const latest = rackTelemetry[0]
      const spectralData = latest ? [
        { name: 'Blue (450nm)', intensity: latest.spectral_blue },
        { name: 'Green (550nm)', intensity: latest.spectral_green },
        { name: 'Red (650nm)', intensity: latest.spectral_red },
        { name: 'NIR (850nm)', intensity: latest.spectral_nir },
      ].filter(s => s.intensity > 0) : []
      return {
        rack,
        telemetry: rackTelemetry,
        spectralData,
      }
    })

  // Racks without devices (no data)
  const racksWithoutDevice = racks.filter(r => !r.device_id)

  return (
    <div className="space-y-6">
      <div className="flex items-center justify-between">
        <h2 className="text-xl font-bold">{t('title')}</h2>
        <button onClick={refetch} className="flex items-center gap-1.5 rounded-lg border border-border px-3 py-1.5 text-sm hover:bg-gray-50">
          <RefreshCw className="h-3.5 w-3.5" />{t('common:actions.refresh')}
        </button>
      </div>

      {loading ? (
        <div className="text-sm text-gray-400">{t('common:actions.loading')}</div>
      ) : racksWithData.length === 0 ? (
        <div className="rounded-xl border border-dashed border-gray-300 bg-gray-50 p-12 text-center">
          <Droplets className="mx-auto h-12 w-12 text-gray-300" />
          <p className="mt-3 text-gray-500">{t('noData', { defaultValue: 'No rack data available' })}</p>
        </div>
      ) : (
        <div className="space-y-4">
          {racksWithData.map(({ rack, telemetry: rackTelemetry, spectralData }) => (
            <RackCard
              key={rack.id}
              rackName={rack.name}
              deviceName={rack.device_name || ''}
              telemetry={rackTelemetry}
              spectralData={spectralData}
            />
          ))}
          {racksWithoutDevice.length > 0 && (
            <div className="rounded-xl border border-dashed border-gray-200 bg-gray-50/50 p-4">
              <p className="text-xs text-gray-400">
                {t('racksNoDevice', { defaultValue: 'Racks without linked device' })}: {racksWithoutDevice.map(r => r.name).join(', ')}
              </p>
            </div>
          )}
        </div>
      )}
    </div>
  )
}
