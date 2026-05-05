import { useState, useEffect } from 'react'
import { useParams, useNavigate } from 'react-router-dom'
import { useTranslation } from 'react-i18next'
import { useOffices } from '../../hooks/useOffices'
import { useRacks, useRackVegetables, useRackEnvironment } from '../../hooks/useRacks'
import { Plus, Edit2, Trash2, ArrowLeft, Thermometer, Droplets, Sun, Layers, Sprout as PlantIcon, MessageCircle, Send, X } from 'lucide-react'
import { apiFetch } from '../../lib/api'
import type { Rack } from '../../types'

export default function RackDetailPage() {
  const { officeId } = useParams<{ officeId: string }>()
  const navigate = useNavigate()
  const { offices } = useOffices()
  const { racks, loading, createRack, updateRack, deleteRack, refetch: refetchRacks } = useRacks(officeId ? Number(officeId) : undefined)
  const { t } = useTranslation('racks')

  const office = offices.find((o) => o.id === Number(officeId))

  const [showForm, setShowForm] = useState(false)
  const [editing, setEditing] = useState<Rack | null>(null)
  const [form, setForm] = useState({ name: '', location: '', layer_count: 3, device_id: '' })
  const [expandedRack, setExpandedRack] = useState<number | null>(null)

  const openCreate = () => {
    setEditing(null)
    setForm({ name: '', location: '', layer_count: 3, device_id: '' })
    setShowForm(true)
  }

  const openEdit = (rack: Rack) => {
    setEditing(rack)
    setForm({ name: rack.name, location: rack.location, layer_count: rack.layer_count, device_id: rack.device_id || '' })
    setShowForm(true)
  }

  const handleSubmit = async (e: React.FormEvent) => {
    e.preventDefault()
    const payload = { ...form, office_id: Number(officeId), device_id: form.device_id || null }
    if (editing) {
      await updateRack(editing.id, payload)
    } else {
      await createRack(payload)
    }
    setShowForm(false)
  }

  const handleDelete = async (id: number) => {
    if (confirm(t('common:actions.delete') + '?')) {
      await deleteRack(id)
    }
  }

  if (loading) return <div className="text-gray-400">{t('common:actions.loading')}</div>

  return (
    <div className="space-y-6">
      <div className="flex items-center justify-between">
        <div className="flex items-center gap-3">
          <button onClick={() => navigate('/racks')} className="rounded-lg p-1.5 hover:bg-gray-100">
            <ArrowLeft className="h-5 w-5 text-gray-500" />
          </button>
          <div>
            <h2 className="text-xl font-bold">{office?.name || t('allOffices')}</h2>
            {office && <p className="text-sm text-gray-500">{office.contact_person} · {office.contact_phone}</p>}
          </div>
        </div>
        <button onClick={openCreate} className="flex items-center gap-1.5 rounded-lg bg-[#00a65a] px-3 py-1.5 text-sm text-white hover:bg-[#00954f]">
          <Plus className="h-4 w-4" />{t('addRack')}
        </button>
      </div>

      {/* Rack Form Modal */}
      {showForm && (
        <div className="fixed inset-0 z-50 flex items-center justify-center bg-black/30">
          <div className="w-full max-w-md rounded-xl bg-white p-6 shadow-xl">
            <h3 className="mb-4 text-lg font-semibold">{editing ? t('editRack') : t('addRack')}</h3>
            <form onSubmit={handleSubmit} className="space-y-3">
              <div>
                <label className="mb-1 block text-sm font-medium text-gray-700">{t('rackName')}</label>
                <input value={form.name} onChange={(e) => setForm({ ...form, name: e.target.value })}
                  className="w-full rounded-lg border border-gray-300 px-3 py-2 text-sm outline-none focus:border-[#00a65a]" required />
              </div>
              <div className="grid grid-cols-2 gap-3">
                <div>
                  <label className="mb-1 block text-sm font-medium text-gray-700">{t('location')}</label>
                  <input value={form.location} onChange={(e) => setForm({ ...form, location: e.target.value })}
                    className="w-full rounded-lg border border-gray-300 px-3 py-2 text-sm outline-none focus:border-[#00a65a]" />
                </div>
                <div>
                  <label className="mb-1 block text-sm font-medium text-gray-700">{t('layerCount')}</label>
                  <input type="number" min={1} max={10} value={form.layer_count} onChange={(e) => setForm({ ...form, layer_count: Number(e.target.value) })}
                    className="w-full rounded-lg border border-gray-300 px-3 py-2 text-sm outline-none focus:border-[#00a65a]" />
                </div>
              </div>
              <div>
                <label className="mb-1 block text-sm font-medium text-gray-700">{t('linkedDevice')}</label>
                <input value={form.device_id} onChange={(e) => setForm({ ...form, device_id: e.target.value })}
                  className="w-full rounded-lg border border-gray-300 px-3 py-2 text-sm outline-none focus:border-[#00a65a]" placeholder="WSD-001" />
              </div>
              <div className="flex justify-end gap-2 pt-2">
                <button type="button" onClick={() => setShowForm(false)} className="rounded-lg border border-gray-300 px-4 py-2 text-sm text-gray-600 hover:bg-gray-50">{t('common:actions.cancel')}</button>
                <button type="submit" className="rounded-lg bg-[#00a65a] px-4 py-2 text-sm text-white hover:bg-[#00954f]">{t('common:actions.save')}</button>
              </div>
            </form>
          </div>
        </div>
      )}

      {/* Racks Grid */}
      {racks.length === 0 ? (
        <div className="rounded-xl border border-dashed border-gray-300 bg-gray-50 p-12 text-center">
          <Layers className="mx-auto h-12 w-12 text-gray-300" />
          <p className="mt-3 text-gray-500">{t('noRacks')}</p>
        </div>
      ) : (
        <div className="space-y-4">
          {racks.map((rack) => (
            <RackCard
              key={rack.id}
              rack={rack}
              expanded={expandedRack === rack.id}
              onToggle={() => setExpandedRack(expandedRack === rack.id ? null : rack.id)}
              onEdit={() => openEdit(rack)}
              onDelete={() => handleDelete(rack.id)}
              refetchRacks={refetchRacks}
            />
          ))}
        </div>
      )}
    </div>
  )
}

function RackCard({ rack, expanded, onToggle, onEdit, onDelete, refetchRacks }: {
  rack: Rack; expanded: boolean; onToggle: () => void; onEdit: () => void; onDelete: () => void; refetchRacks: () => void
}) {
  const { t } = useTranslation('racks')
  const { offices } = useOffices()
  const { vegetables, loading: vegLoading, addVegetable, deleteVegetable } = useRackVegetables(expanded ? rack.id : undefined)
  const { records: envRecords, loading: envLoading } = useRackEnvironment(expanded ? rack.id : undefined)
  const [showAddVeg, setShowAddVeg] = useState(false)
  const [showWhatsApp, setShowWhatsApp] = useState(false)
  const [vegForm, setVegForm] = useState({ layer_number: 1, variety: '', quantity: 1 })

  const statusColors: Record<string, string> = {
    active: 'bg-green-500', inactive: 'bg-gray-400', maintenance: 'bg-blue-500',
  }
  const statusLabels: Record<string, string> = {
    active: t('common:status.active'), inactive: t('common:status.inactive'), maintenance: t('common:status.maintenance'),
  }

  const handleAddVeg = async (e: React.FormEvent) => {
    e.preventDefault()
    await addVegetable({ ...vegForm, rack_id: rack.id })
    setShowAddVeg(false)
    setVegForm({ layer_number: 1, variety: '', quantity: 1 })
    refetchRacks()
  }

  const latestEnv = envRecords[0]

  // Group vegetables by layer
  const layerMap = new Map<number, typeof vegetables>()
  vegetables.forEach((v) => {
    if (!layerMap.has(v.layer_number)) layerMap.set(v.layer_number, [])
    layerMap.get(v.layer_number)!.push(v)
  })

  return (
    <div className="rounded-xl border border-border bg-white shadow-sm overflow-hidden">
      {/* Rack Header */}
      <div className="flex items-center justify-between p-4 cursor-pointer hover:bg-gray-50" onClick={onToggle}>
        <div className="flex items-center gap-3">
          <span className={`h-2.5 w-2.5 rounded-full ${statusColors[rack.status]}`} />
          <div>
            <h3 className="font-semibold text-gray-900">{rack.name}</h3>
            <p className="text-xs text-gray-500">{rack.location} · {rack.layer_count} {t('layerCount').toLowerCase()} · {rack.device_name || t('noDevice')}</p>
          </div>
        </div>
        <div className="flex items-center gap-2">
          <span className="rounded-full bg-gray-100 px-2 py-0.5 text-xs text-gray-600">{statusLabels[rack.status]}</span>
          <button onClick={(e) => { e.stopPropagation(); setShowWhatsApp(true) }} className="rounded p-1 text-green-500 hover:bg-green-50" title="Send WhatsApp">
            <MessageCircle className="h-3.5 w-3.5" />
          </button>
          <button onClick={(e) => { e.stopPropagation(); onEdit() }} className="rounded p-1 text-gray-400 hover:text-gray-600"><Edit2 className="h-3.5 w-3.5" /></button>
          <button onClick={(e) => { e.stopPropagation(); onDelete() }} className="rounded p-1 text-gray-400 hover:text-red-500"><Trash2 className="h-3.5 w-3.5" /></button>
        </div>
      </div>

      {/* WhatsApp Dialog */}
      {showWhatsApp && (
        <WhatsAppDialog
          rack={rack}
          office={offices.find(o => o.id === rack.office_id)}
          onClose={() => setShowWhatsApp(false)}
        />
      )}

      {/* Expanded Content */}
      {expanded && (
        <div className="border-t border-border p-4 space-y-4">
          {/* Environment Panel */}
          <div>
            <h4 className="text-sm font-semibold text-gray-700 mb-2">{t('environment')}</h4>
            {envLoading ? <p className="text-xs text-gray-400">{t('common:actions.loading')}</p> :
              latestEnv ? (
                <div className="grid grid-cols-5 gap-3">
                  <EnvMetric icon={Thermometer} label={t('temperature')} value={latestEnv.temperature} unit="°C" color="#E91E63" />
                  <EnvMetric icon={Droplets} label={t('humidity')} value={latestEnv.humidity} unit="%" color="#2196F3" />
                  <EnvMetric icon={Sun} label={t('lightLevel')} value={latestEnv.light_level} unit="lux" color="#FF9800" />
                  <EnvMetric icon={Droplets} label="pH" value={latestEnv.ph} unit="" color="#4CAF50" />
                  <EnvMetric icon={Droplets} label="EC" value={latestEnv.ec} unit="μS" color="#9C27B0" />
                </div>
              ) : <p className="text-xs text-gray-400">{t('noEnvData')}</p>
            }
          </div>

          {/* Vegetable Counter by Layers */}
          <div>
            <div className="flex items-center justify-between mb-2">
              <h4 className="text-sm font-semibold text-gray-700">{t('vegetables')}</h4>
              <button onClick={() => setShowAddVeg(!showAddVeg)} className="flex items-center gap-1 rounded-lg bg-[#e8f5e9] px-2 py-1 text-xs text-[#2E7D32] hover:bg-[#c8e6c9]">
                <Plus className="h-3 w-3" />{t('addVegetable')}
              </button>
            </div>

            {showAddVeg && (
              <form onSubmit={handleAddVeg} className="mb-3 flex items-end gap-2 rounded-lg border border-[#00a65a]/30 bg-[#e8f5e9]/50 p-3">
                <div>
                  <label className="block text-[10px] text-gray-500">{t('layer').replace(' {{number}}', '')}</label>
                  <input type="number" min={1} max={rack.layer_count} value={vegForm.layer_number} onChange={(e) => setVegForm({ ...vegForm, layer_number: Number(e.target.value) })}
                    className="w-16 rounded border border-gray-300 px-2 py-1 text-sm" />
                </div>
                <div>
                  <label className="block text-[10px] text-gray-500">{t('variety')}</label>
                  <input value={vegForm.variety} onChange={(e) => setVegForm({ ...vegForm, variety: e.target.value })}
                    className="w-28 rounded border border-gray-300 px-2 py-1 text-sm" required />
                </div>
                <div>
                  <label className="block text-[10px] text-gray-500">{t('quantity')}</label>
                  <input type="number" min={1} value={vegForm.quantity} onChange={(e) => setVegForm({ ...vegForm, quantity: Number(e.target.value) })}
                    className="w-16 rounded border border-gray-300 px-2 py-1 text-sm" />
                </div>
                <button type="submit" className="rounded bg-[#00a65a] px-3 py-1 text-sm text-white">{t('common:actions.add')}</button>
              </form>
            )}

            {vegLoading ? <p className="text-xs text-gray-400">{t('common:actions.loading')}</p> :
              Array.from({ length: rack.layer_count }, (_, i) => i + 1).map((layer) => (
                <div key={layer} className="mb-2 rounded-lg border border-gray-100 bg-gray-50/50 p-2">
                  <p className="text-xs font-medium text-gray-500 mb-1">{t('layer', { number: layer })}</p>
                  {layerMap.has(layer) ? (
                    <div className="flex flex-wrap gap-1.5">
                      {layerMap.get(layer)!.map((v) => (
                        <span key={v.id} className="inline-flex items-center gap-1 rounded-full bg-white px-2 py-0.5 text-xs border border-gray-200">
                          <PlantIcon className="h-3 w-3 text-green-500" />
                          {v.variety} ×{v.quantity}
                          <button onClick={() => { deleteVegetable(v.id); refetchRacks() }} className="text-gray-400 hover:text-red-500">×</button>
                        </span>
                      ))}
                    </div>
                  ) : (
                    <p className="text-[10px] text-gray-400">{t('noVegetables')}</p>
                  )}
                </div>
              ))
            }
          </div>
        </div>
      )}
    </div>
  )
}

function EnvMetric({ icon: Icon, label, value, unit, color }: {
  icon: React.ElementType; label: string; value: number | null; unit: string; color: string
}) {
  return (
    <div className="rounded-lg border border-gray-100 bg-gray-50 p-2 text-center">
      <Icon className="mx-auto h-4 w-4 mb-1" style={{ color }} />
      <p className="text-[10px] text-gray-500">{label}</p>
      <p className="text-sm font-bold text-gray-800">{value !== null ? `${value}${unit}` : '-'}</p>
    </div>
  )
}

function WhatsAppDialog({ rack, office, onClose }: {
  rack: Rack; office: { whatsapp_number?: string; name?: string } | undefined; onClose: () => void
}) {
  const { t } = useTranslation('racks')
  const [message, setMessage] = useState('')
  const [phone, setPhone] = useState(office?.whatsapp_number || '')
  const [sending, setSending] = useState(false)
  const [sent, setSent] = useState(false)

  const handleSend = async () => {
    if (!phone || !message) return
    setSending(true)
    try {
      await apiFetch('/workbuddy/send-whatsapp', {
        method: 'POST',
        body: JSON.stringify({ phone, message }),
      })
      setSent(true)
      setTimeout(onClose, 1500)
    } catch {
      alert('Failed to send message')
    } finally {
      setSending(false)
    }
  }

  return (
    <div className="border-t border-border bg-green-50/50 p-4">
      <div className="flex items-center justify-between mb-3">
        <div className="flex items-center gap-2">
          <MessageCircle className="h-4 w-4 text-green-600" />
          <span className="text-sm font-medium text-gray-700">
            {t('sendWhatsApp', { defaultValue: 'Send WhatsApp' })} — {rack.name}
          </span>
        </div>
        <button onClick={onClose} className="rounded p-1 text-gray-400 hover:text-gray-600">
          <X className="h-4 w-4" />
        </button>
      </div>
      <div className="space-y-2">
        <div>
          <label className="block text-[10px] text-gray-500 mb-1">{t('phone', { defaultValue: 'Phone' })}</label>
          <input value={phone} onChange={(e) => setPhone(e.target.value)}
            className="w-full rounded-lg border border-gray-200 px-3 py-1.5 text-sm outline-none focus:border-[#00a65a]"
            placeholder="85291234567" />
        </div>
        <div>
          <label className="block text-[10px] text-gray-500 mb-1">{t('message', { defaultValue: 'Message' })}</label>
          <textarea value={message} onChange={(e) => setMessage(e.target.value)} rows={3}
            className="w-full rounded-lg border border-gray-200 px-3 py-1.5 text-sm outline-none focus:border-[#00a65a] resize-none"
            placeholder={`${t('aboutRack', { defaultValue: 'About rack' })} ${rack.name}...`} />
        </div>
        <div className="flex justify-end gap-2">
          <button onClick={onClose} className="rounded-lg border border-gray-300 px-3 py-1.5 text-xs text-gray-600 hover:bg-gray-50">
            {t('common:actions.cancel')}
          </button>
          <button onClick={handleSend} disabled={sending || !phone || !message}
            className="flex items-center gap-1 rounded-lg bg-[#25D366] px-3 py-1.5 text-xs text-white hover:bg-[#20BD5A] disabled:opacity-50">
            <Send className="h-3 w-3" />
            {sent ? t('sent', { defaultValue: 'Sent!' }) : sending ? t('sending', { defaultValue: 'Sending...' }) : t('send', { defaultValue: 'Send' })}
          </button>
        </div>
      </div>
    </div>
  )
}
