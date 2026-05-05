import { useState, useEffect, useCallback, useMemo } from 'react'
import { Sprout, Scissors, Plus, Filter, X, TrendingUp, Package, Leaf, Calendar } from 'lucide-react'
import { useOffice } from '../context/OfficeContext'
import { useOffices } from '../hooks/useOffices'
import { useRacks } from '../hooks/useRacks'
import { apiFetch } from '../lib/api'

interface CropBatch {
  id: number
  office_id: number
  rack_id: number | null
  layer_number: number | null
  variety: string
  quantity: number
  unit: string
  status: 'growing' | 'ready' | 'harvested' | 'failed'
  seeded_at: number
  expected_harvest_days: number
  notes: string
  office_name?: string
  rack_name?: string
}

interface HarvestLog {
  id: number
  batch_id: number
  quantity: number
  unit: string
  quality: 'excellent' | 'good' | 'fair' | 'poor'
  notes: string
  harvested_at: number
  variety?: string
  office_name?: string
  rack_name?: string
}

const statusConfig = {
  growing: { label: '生長中 / Growing', color: 'bg-green-100 text-green-700', dot: 'bg-green-500' },
  ready: { label: '可收成 / Ready', color: 'bg-amber-100 text-amber-700', dot: 'bg-amber-500' },
  harvested: { label: '已收成 / Harvested', color: 'bg-blue-100 text-blue-700', dot: 'bg-blue-500' },
  failed: { label: '失敗 / Failed', color: 'bg-red-100 text-red-700', dot: 'bg-red-500' },
}

const qualityConfig = {
  excellent: { label: '優 Excellent', color: 'text-green-600' },
  good: { label: '良 Good', color: 'text-blue-600' },
  fair: { label: '可 Fair', color: 'text-amber-600' },
  poor: { label: '差 Poor', color: 'text-red-600' },
}

function formatDate(ts: number) {
  return new Date(ts * 1000).toLocaleDateString('zh-TW', { month: 'short', day: 'numeric' })
}

function daysSince(ts: number) {
  return Math.floor((Date.now() / 1000 - ts) / 86400)
}

function AddBatchForm({ offices, racks, userRole, lockedOfficeId, onSaved }: {
  offices: { id: number; name: string }[]
  racks: { id: number; name: string; office_id: number }[]
  userRole: string
  lockedOfficeId: number | null
  onSaved: () => void
}) {
  const [form, setForm] = useState({
    office_id: userRole === 'superadmin' ? '' : (lockedOfficeId?.toString() || ''),
    rack_id: '',
    variety: '',
    quantity: '',
    expected_harvest_days: '10',
    notes: '',
  })
  const [saving, setSaving] = useState(false)

  const filteredRacks = form.office_id
    ? racks.filter(r => r.office_id === Number(form.office_id))
    : racks

  const handleSubmit = async (e: React.FormEvent) => {
    e.preventDefault()
    if (!form.variety.trim()) {
      alert('請輸入品種 / Please enter variety')
      return
    }
    setSaving(true)
    try {
      const payload: Record<string, unknown> = {
        variety: form.variety.trim(),
        quantity: Number(form.quantity) || 0,
        unit: '株',
        expected_harvest_days: Number(form.expected_harvest_days) || 30,
        notes: form.notes,
      }
      if (form.office_id) payload.office_id = Number(form.office_id)
      if (form.rack_id) payload.rack_id = Number(form.rack_id)

      await apiFetch('/crop-batches', {
        method: 'POST',
        body: JSON.stringify(payload),
      })
      onSaved()
    } catch (err: any) {
      alert('入苗失敗：' + (err.message || '未知錯誤'))
    }
    setSaving(false)
  }

  return (
    <form onSubmit={handleSubmit} className="rounded-xl border border-green-200 bg-green-50/50 p-5">
      <h3 className="text-sm font-semibold text-gray-700 mb-3 flex items-center gap-2">
        <Sprout className="h-4 w-4 text-green-600" />入苗記錄 / New Seedling Entry
      </h3>
      <div className="grid grid-cols-2 gap-3 lg:grid-cols-4">
        {userRole === 'superadmin' && (
          <div>
            <label className="block text-[10px] text-gray-500 mb-1">Office 辦公室</label>
            <select value={form.office_id} onChange={e => setForm({ ...form, office_id: e.target.value, rack_id: '' })}
              className="w-full rounded border border-gray-300 px-2 py-1.5 text-sm">
              <option value="">-- Select --</option>
              {offices.map(o => <option key={o.id} value={o.id}>{o.name}</option>)}
            </select>
          </div>
        )}
        <div>
          <label className="block text-[10px] text-gray-500 mb-1">Rack 耕架</label>
          <select value={form.rack_id} onChange={e => setForm({ ...form, rack_id: e.target.value })}
            className="w-full rounded border border-gray-300 px-2 py-1.5 text-sm">
            <option value="">-- Select --</option>
            {filteredRacks.map(r => <option key={r.id} value={r.id}>{r.name}</option>)}
          </select>
        </div>
        <div>
          <label className="block text-[10px] text-gray-500 mb-1">Variety 品種</label>
          <input value={form.variety} onChange={e => setForm({ ...form, variety: e.target.value })}
            className="w-full rounded border border-gray-300 px-2 py-1.5 text-sm" required placeholder="例：生菜" />
        </div>
        <div>
          <label className="block text-[10px] text-gray-500 mb-1">Quantity 數量</label>
          <input type="number" value={form.quantity} onChange={e => setForm({ ...form, quantity: e.target.value })}
            className="w-full rounded border border-gray-300 px-2 py-1.5 text-sm" required placeholder="0" min="0" />
        </div>
        <div>
          <label className="block text-[10px] text-gray-500 mb-1">預計天數 Days</label>
          <input type="number" value={form.expected_harvest_days} onChange={e => setForm({ ...form, expected_harvest_days: e.target.value })}
            className="w-full rounded border border-gray-300 px-2 py-1.5 text-sm" min="1" />
        </div>
        <div>
          <label className="block text-[10px] text-gray-500 mb-1">Notes 備註</label>
          <input value={form.notes} onChange={e => setForm({ ...form, notes: e.target.value })}
            className="w-full rounded border border-gray-300 px-2 py-1.5 text-sm" />
        </div>
      </div>
      <div className="flex gap-2 mt-3">
        <button type="submit" disabled={saving}
          className="rounded bg-green-600 px-4 py-1.5 text-sm text-white hover:bg-green-700 disabled:opacity-50">
          {saving ? '入苗中...' : '入苗 / Plant'}
        </button>
      </div>
    </form>
  )
}

function HarvestModal({ batch, onSaved, onClose }: {
  batch: CropBatch
  onSaved: (msg: string) => void
  onClose: () => void
}) {
  const [form, setForm] = useState({
    quantity: '',
    quality: 'good' as string,
    notes: '',
  })
  const [saving, setSaving] = useState(false)

  const handleSubmit = async () => {
    if (!form.quantity || Number(form.quantity) <= 0) {
      alert('請輸入收成數量 / Please enter harvest quantity')
      return
    }
    setSaving(true)
    try {
      await apiFetch('/harvests', {
        method: 'POST',
        body: JSON.stringify({
          batch_id: batch.id,
          quantity: Number(form.quantity) || 0,
          unit: batch.unit || '株',
          quality: form.quality,
          notes: form.notes,
        }),
      })
      setSaving(false)
      onSaved('收成成功！ / Harvest recorded!')
      onClose()
    } catch (err: any) {
      setSaving(false)
      alert('收成記錄失敗：' + (err.message || '未知錯誤'))
    }
  }

  return (
    <div className="fixed inset-0 z-50 flex items-center justify-center bg-black/40" onClick={onClose}>
      <div className="w-full max-w-md rounded-xl bg-white p-6 shadow-xl" onClick={e => e.stopPropagation()}>
        <div className="flex items-center justify-between mb-4">
          <h3 className="text-lg font-bold flex items-center gap-2">
            <Scissors className="h-5 w-5 text-amber-500" />收成 / Harvest
          </h3>
          <button onClick={onClose} className="text-gray-400 hover:text-gray-600"><X className="h-5 w-5" /></button>
        </div>
        <div className="mb-4 rounded-lg bg-gray-50 p-3 text-sm">
          <p><strong>{batch.variety}</strong> — {batch.quantity}{batch.unit}</p>
          <p className="text-xs text-gray-500">{batch.rack_name || 'No rack'} · 種植 {daysSince(batch.seeded_at)} 天</p>
        </div>
        <div className="space-y-3">
          <div>
            <label className="block text-xs text-gray-500 mb-1">收成數量 / Harvest Quantity</label>
              <input type="number" value={form.quantity} onChange={e => setForm({ ...form, quantity: e.target.value })}
                className="w-full rounded border border-gray-300 px-3 py-2 text-sm" min="0" />
          </div>
          <div>
            <label className="block text-xs text-gray-500 mb-1">品質 / Quality</label>
            <select value={form.quality} onChange={e => setForm({ ...form, quality: e.target.value })}
              className="w-full rounded border border-gray-300 px-3 py-2 text-sm">
              <option value="excellent">優 Excellent</option>
              <option value="good">良 Good</option>
              <option value="fair">可 Fair</option>
              <option value="poor">差 Poor</option>
            </select>
          </div>
          <div>
            <label className="block text-xs text-gray-500 mb-1">備註 / Notes</label>
            <input value={form.notes} onChange={e => setForm({ ...form, notes: e.target.value })}
              className="w-full rounded border border-gray-300 px-3 py-2 text-sm" placeholder="Optional" />
          </div>
        </div>
        <div className="flex gap-2 mt-5">
          <button type="button" onClick={handleSubmit} disabled={saving}
            className="rounded-lg bg-amber-500 px-4 py-2 text-sm text-white hover:bg-amber-600 disabled:opacity-50">
            {saving ? '記錄中...' : '確認收成 / Confirm Harvest'}
          </button>
          <button type="button" onClick={onClose}
            className="rounded-lg border border-gray-300 px-4 py-2 text-sm text-gray-600 hover:bg-gray-50">
            取消 / Cancel
          </button>
        </div>
      </div>
    </div>
  )
}

function CropCalendar({ batches }: { batches: CropBatch[] }) {
  const [baseDate, setBaseDate] = useState(new Date())

  const year = baseDate.getFullYear()
  const month = baseDate.getMonth()
  const monthName = baseDate.toLocaleDateString('zh-TW', { year: 'numeric', month: 'long' })

  const firstDay = new Date(year, month, 1).getDay() // 0=Sun
  const daysInMonth = new Date(year, month + 1, 0).getDate()

  // Map batches to dates
  const dateMap = useMemo(() => {
    const map = new Map<number, { seedlings: CropBatch[]; harvestExpected: CropBatch[] }>()
    batches.forEach(b => {
      // Seeded date
      const seededDate = new Date(b.seeded_at * 1000)
      if (seededDate.getFullYear() === year && seededDate.getMonth() === month) {
        const day = seededDate.getDate()
        if (!map.has(day)) map.set(day, { seedlings: [], harvestExpected: [] })
        map.get(day)!.seedlings.push(b)
      }
      // Expected harvest date
      const harvestTs = b.seeded_at + b.expected_harvest_days * 86400
      const harvestDate = new Date(harvestTs * 1000)
      if (harvestDate.getFullYear() === year && harvestDate.getMonth() === month) {
        const day = harvestDate.getDate()
        if (!map.has(day)) map.set(day, { seedlings: [], harvestExpected: [] })
        map.get(day)!.harvestExpected.push(b)
      }
    })
    return map
  }, [batches, year, month])

  const prevMonth = () => setBaseDate(new Date(year, month - 1, 1))
  const nextMonth = () => setBaseDate(new Date(year, month + 1, 1))
  const thisMonth = () => setBaseDate(new Date())

  const today = new Date()
  const isToday = (day: number) =>
    today.getFullYear() === year && today.getMonth() === month && today.getDate() === day

  const weekdays = ['日', '一', '二', '三', '四', '五', '六']

  return (
    <div className="rounded-xl border border-border bg-white p-5">
      <div className="flex items-center justify-between mb-4">
        <h3 className="text-sm font-semibold text-gray-700 flex items-center gap-2">
          <Calendar className="h-4 w-4 text-green-600" />入苗收成日曆
        </h3>
        <div className="flex items-center gap-2">
          <button onClick={prevMonth} className="rounded border border-gray-200 px-2 py-1 text-xs hover:bg-gray-50">&lt;</button>
          <span className="text-sm font-medium min-w-[120px] text-center">{monthName}</span>
          <button onClick={nextMonth} className="rounded border border-gray-200 px-2 py-1 text-xs hover:bg-gray-50">&gt;</button>
          <button onClick={thisMonth} className="rounded border border-gray-200 px-2 py-1 text-xs hover:bg-gray-50">今天</button>
        </div>
      </div>
      <div className="grid grid-cols-7 gap-px bg-gray-100 rounded-lg overflow-hidden">
        {weekdays.map(d => (
          <div key={d} className="bg-gray-50 py-1.5 text-center text-[10px] font-medium text-gray-500">{d}</div>
        ))}
        {Array.from({ length: firstDay }, (_, i) => (
          <div key={`empty-${i}`} className="bg-white p-1.5 min-h-[64px]" />
        ))}
        {Array.from({ length: daysInMonth }, (_, i) => {
          const day = i + 1
          const info = dateMap.get(day)
          return (
            <div key={day} className={`bg-white p-1.5 min-h-[64px] ${isToday(day) ? 'ring-2 ring-green-400 ring-inset' : ''}`}>
              <div className={`text-[10px] font-medium ${isToday(day) ? 'text-green-600' : 'text-gray-400'}`}>{day}</div>
              {info && (
                <div className="mt-0.5 space-y-0.5">
                  {info.seedlings.map(b => (
                    <div key={`s-${b.id}`} className="flex items-center gap-0.5 rounded bg-green-100 px-1 py-px">
                      <Sprout className="h-2.5 w-2.5 text-green-600 shrink-0" />
                      <span className="text-[9px] text-green-700 truncate">{b.variety}</span>
                    </div>
                  ))}
                  {info.harvestExpected.map(b => (
                    <div key={`h-${b.id}`} className="flex items-center gap-0.5 rounded bg-amber-100 px-1 py-px">
                      <Scissors className="h-2.5 w-2.5 text-amber-600 shrink-0" />
                      <span className="text-[9px] text-amber-700 truncate">{b.variety}</span>
                    </div>
                  ))}
                </div>
              )}
            </div>
          )
        })}
      </div>
      <div className="flex items-center gap-4 mt-3 text-[10px] text-gray-500">
        <span className="flex items-center gap-1"><span className="inline-block h-2 w-2 rounded bg-green-400" />入苗</span>
        <span className="flex items-center gap-1"><span className="inline-block h-2 w-2 rounded bg-amber-400" />預計收成</span>
        <span className="flex items-center gap-1"><span className="inline-block h-2 w-2 rounded ring-2 ring-green-400" />今天</span>
      </div>
    </div>
  )
}

export default function CropManagementPage() {
  const { selectedOfficeId, userRole, lockedOfficeId } = useOffice()
  const { offices } = useOffices()
  const { racks } = useRacks()
  const [batches, setBatches] = useState<CropBatch[]>([])
  const [harvests, setHarvests] = useState<HarvestLog[]>([])
  const [loading, setLoading] = useState(true)
  const [tab, setTab] = useState<'seedlings' | 'harvests'>('seedlings')
  const [statusFilter, setStatusFilter] = useState<string>('')
  const [showAdd, setShowAdd] = useState(false)
  const [harvestBatch, setHarvestBatch] = useState<CropBatch | null>(null)
  const [successMsg, setSuccessMsg] = useState('')

  const fetchData = useCallback(async () => {
    setLoading(true)
    try {
      const officeParam = userRole === 'superadmin' && selectedOfficeId ? `?office_id=${selectedOfficeId}` : ''
      const batchUrl = `/crop-batches${statusFilter ? `${officeParam ? '&' : '?'}status=${statusFilter}` : officeParam}`
      const harvestUrl = `/harvests${officeParam}`

      const [batchData, harvestData] = await Promise.all([
        apiFetch<CropBatch[]>(batchUrl).catch(err => { console.error('crop-batches error:', err); return [] as CropBatch[] }),
        apiFetch<HarvestLog[]>(harvestUrl).catch(err => { console.error('harvests error:', err); return [] as HarvestLog[] }),
      ])
      setBatches(batchData)
      setHarvests(harvestData)
    } catch (err) { console.error('fetchData error:', err) }
    setLoading(false)
  }, [selectedOfficeId, userRole, statusFilter])

  useEffect(() => { fetchData() }, [fetchData])

  // Stats
  const growing = batches.filter(b => b.status === 'growing').length
  const ready = batches.filter(b => b.status === 'ready').length
  const totalHarvested = harvests.reduce((sum, h) => sum + h.quantity, 0)

  return (
    <div className="space-y-6">
      <div className="flex items-center justify-between">
        <div>
          <h2 className="text-xl font-bold">Crop Management / 農作物管理</h2>
          <p className="text-sm text-gray-500 mt-1">Seedling Entry & Harvest Tracking / 入苗及收成管理</p>
        </div>
        {(
          <button onClick={() => setShowAdd(!showAdd)}
            className="flex items-center gap-1.5 rounded-lg bg-green-600 px-3 py-1.5 text-sm text-white hover:bg-green-700">
            <Plus className="h-4 w-4" />入苗 / New Seedling
          </button>
        )}
      </div>

      {/* Success message */}
      {successMsg && (
        <div className="rounded-lg bg-green-100 border border-green-300 px-4 py-2.5 text-sm text-green-800 font-medium">
          {successMsg}
        </div>
      )}

      {/* Stats */}
      <div className="grid grid-cols-3 gap-4">
        <div className="rounded-xl border border-green-200 bg-green-50 p-4">
          <div className="flex items-center gap-2">
            <Sprout className="h-5 w-5 text-green-600" />
            <span className="text-sm text-gray-600">生長中 / Growing</span>
          </div>
          <p className="mt-1 text-2xl font-bold text-green-700">{growing}</p>
        </div>
        <div className="rounded-xl border border-amber-200 bg-amber-50 p-4">
          <div className="flex items-center gap-2">
            <Leaf className="h-5 w-5 text-amber-600" />
            <span className="text-sm text-gray-600">可收成 / Ready</span>
          </div>
          <p className="mt-1 text-2xl font-bold text-amber-700">{ready}</p>
        </div>
        <div className="rounded-xl border border-blue-200 bg-blue-50 p-4">
          <div className="flex items-center gap-2">
            <Package className="h-5 w-5 text-blue-600" />
            <span className="text-sm text-gray-600">已收成 / Harvested</span>
          </div>
          <p className="mt-1 text-2xl font-bold text-blue-700">{totalHarvested}</p>
        </div>
      </div>

      {/* Tabs */}
      <div className="flex items-center gap-1 border-b border-border">
        <button onClick={() => setTab('seedlings')}
          className={`px-4 py-2 text-sm font-medium border-b-2 transition-colors ${
            tab === 'seedlings' ? 'border-green-600 text-green-700 bg-green-50' : 'border-transparent text-gray-500 hover:text-gray-700'
          }`}>
          <Sprout className="inline h-4 w-4 mr-1" />入苗記錄 ({batches.length})
        </button>
        <button onClick={() => setTab('harvests')}
          className={`px-4 py-2 text-sm font-medium border-b-2 transition-colors ${
            tab === 'harvests' ? 'border-amber-500 text-amber-700 bg-amber-50' : 'border-transparent text-gray-500 hover:text-gray-700'
          }`}>
          <Scissors className="inline h-4 w-4 mr-1" />收成記錄 ({harvests.length})
        </button>
        {tab === 'seedlings' && (
          <div className="ml-auto flex items-center gap-1">
            <Filter className="h-3.5 w-3.5 text-gray-400" />
            <select value={statusFilter} onChange={e => setStatusFilter(e.target.value)}
              className="rounded border border-gray-200 px-2 py-1 text-xs">
              <option value="">All / 全部</option>
              <option value="growing">Growing / 生長中</option>
              <option value="ready">Ready / 可收成</option>
              <option value="harvested">Harvested / 已收成</option>
              <option value="failed">Failed / 失敗</option>
            </select>
          </div>
        )}
      </div>

      {/* Add seedling form */}
      {showAdd && (
        <AddBatchForm
          offices={offices}
          racks={racks}
          userRole={userRole || 'staff'}
          lockedOfficeId={lockedOfficeId ?? null}
          onSaved={() => { setShowAdd(false); setSuccessMsg('入苗成功！'); fetchData(); setTimeout(() => setSuccessMsg(''), 3000) }}
        />
      )}

      {/* Content */}
      {loading ? (
        <div className="text-gray-400 text-sm">Loading...</div>
      ) : tab === 'seedlings' ? (
        batches.length === 0 ? (
          <div className="rounded-xl border border-dashed border-gray-300 p-8 text-center text-gray-400">
            <Sprout className="mx-auto h-8 w-8 mb-2" />
            <p>No seedling records / 暫無入苗記錄</p>
          </div>
        ) : (
          <div className="space-y-2">
            {batches.map(batch => {
              const cfg = statusConfig[batch.status]
              const days = daysSince(batch.seeded_at)
              const progress = Math.min(100, Math.round((days / batch.expected_harvest_days) * 100))
              return (
                <div key={batch.id} className="rounded-xl border border-border bg-white p-4 hover:shadow-sm transition-shadow">
                  <div className="flex items-start justify-between">
                    <div className="flex items-start gap-3">
                      <div className={`mt-0.5 flex h-8 w-8 items-center justify-center rounded-full text-white text-xs font-bold ${cfg.dot}`}>
                        <Sprout className="h-4 w-4" />
                      </div>
                      <div>
                        <div className="flex items-center gap-2">
                          <h4 className="font-medium text-gray-800">{batch.variety}</h4>
                          <span className={`rounded-full px-2 py-0.5 text-[10px] font-medium ${cfg.color}`}>
                            {cfg.label}
                          </span>
                        </div>
                        <p className="text-xs text-gray-500 mt-0.5">
                          {batch.quantity}{batch.unit}
                          {batch.rack_name && ` · ${batch.rack_name}`}
                          {batch.office_name && ` · ${batch.office_name}`}
                        </p>
                        <p className="text-xs text-gray-400 mt-0.5">
                          入苗: {formatDate(batch.seeded_at)} · 第 {days} 天 / 預計 {batch.expected_harvest_days} 天
                        </p>
                        {batch.notes && <p className="text-xs text-gray-400 mt-0.5 italic">{batch.notes}</p>}
                      </div>
                    </div>
                    <div className="flex items-center gap-2">
                      {batch.status !== 'harvested' && (
                        <button onClick={() => setHarvestBatch(batch)}
                          className="rounded-lg border border-amber-300 bg-amber-50 px-3 py-1 text-xs text-amber-700 hover:bg-amber-100 flex items-center gap-1">
                          <Scissors className="h-3 w-3" />收成
                        </button>
                      )}
                      {batch.status === 'growing' && days >= batch.expected_harvest_days * 0.8 && (
                        <button onClick={async () => {
                          try {
                          await apiFetch(`/crop-batches/${batch.id}`, {
                            method: 'PATCH',
                            body: JSON.stringify({ status: 'ready' }),
                          })
                          } catch (err: any) { alert('更新失敗：' + (err.message || '')) }
                          fetchData()
                        }}
                          className="rounded-lg border border-green-300 bg-green-50 px-2 py-1 text-[10px] text-green-700 hover:bg-green-100">
                          標記可收成
                        </button>
                      )}
                      {(
                        <button onClick={async () => {
                          if (!confirm('確定刪除？')) return
                          try {
                          await apiFetch(`/crop-batches/${batch.id}`, { method: 'DELETE' })
                          } catch (err: any) { alert('刪除失敗：' + (err.message || '')) }
                          fetchData()
                        }}
                          className="rounded p-1 text-gray-300 hover:text-red-400">
                          <X className="h-3.5 w-3.5" />
                        </button>
                      )}
                    </div>
                  </div>
                  {/* Progress bar */}
                  <div className="mt-3 h-1.5 rounded-full bg-gray-100 overflow-hidden">
                    <div
                      className={`h-full rounded-full transition-all ${
                        progress >= 100 ? 'bg-amber-500' : progress >= 80 ? 'bg-green-400' : 'bg-green-300'
                      }`}
                      style={{ width: `${progress}%` }}
                    />
                  </div>
                </div>
              )
            })}
          </div>
        )
      ) : (
        harvests.length === 0 ? (
          <div className="rounded-xl border-2 border-dashed border-amber-200 bg-amber-50/50 p-12 text-center">
            <Scissors className="mx-auto h-10 w-10 mb-3 text-amber-300" />
            <p className="text-amber-600 font-medium">暫無收成記錄 / No Harvest Records</p>
            <p className="text-xs text-amber-400 mt-2">在「入苗記錄」中按「收成」按鈕來記錄收成</p>
          </div>
        ) : (
          <div className="rounded-xl border border-border bg-white overflow-hidden">
            <table className="w-full text-sm">
              <thead>
                <tr className="border-b border-border bg-gray-50 text-left text-xs text-gray-500">
                  <th className="px-4 py-2">Date / 日期</th>
                  <th className="px-4 py-2">Variety / 品種</th>
                  <th className="px-4 py-2">Quantity / 數量</th>
                  <th className="px-4 py-2">Quality / 品質</th>
                  <th className="px-4 py-2">Rack / 耕架</th>
                  <th className="px-4 py-2">Office / 辦公室</th>
                  <th className="px-4 py-2">Notes / 備註</th>
                </tr>
              </thead>
              <tbody className="divide-y divide-border">
                {harvests.map(h => (
                  <tr key={h.id} className="hover:bg-gray-50">
                    <td className="px-4 py-2.5 text-gray-600">{formatDate(h.harvested_at)}</td>
                    <td className="px-4 py-2.5 font-medium text-gray-800">{h.variety || '-'}</td>
                    <td className="px-4 py-2.5">{h.quantity}{h.unit}</td>
                    <td className={`px-4 py-2.5 font-medium ${qualityConfig[h.quality]?.color || ''}`}>
                      {qualityConfig[h.quality]?.label || h.quality}
                    </td>
                    <td className="px-4 py-2.5 text-gray-500">{h.rack_name || '-'}</td>
                    <td className="px-4 py-2.5 text-gray-500">{h.office_name || '-'}</td>
                    <td className="px-4 py-2.5 text-gray-400 text-xs">{h.notes || '-'}</td>
                  </tr>
                ))}
              </tbody>
            </table>
          </div>
        )
      )}

      {/* Harvest modal */}
      {harvestBatch && (
        <HarvestModal batch={harvestBatch} onSaved={(msg) => { setSuccessMsg(msg); fetchData(); setTimeout(() => setSuccessMsg(''), 3000) }} onClose={() => setHarvestBatch(null)} />
      )}

      {/* Calendar */}
      <CropCalendar batches={batches} />
    </div>
  )
}
