import { useState, useEffect, useCallback } from 'react'
import { useTranslation } from 'react-i18next'
import { Wifi, Bot, Radio, Bell, Wrench, Info, Zap, Users, Plus, Trash2 } from 'lucide-react'
import { useAutomations } from '../hooks/useRacks'
import { useOffice } from '../context/OfficeContext'
import { useChineseText } from '../hooks/useChineseText'
import { apiFetch } from '../lib/api'
import { timeAgo } from '../lib/utils'
import type { User } from '../types'

function SettingsSection({ title, icon: Icon, children }: {
  title: string; icon: React.ElementType; children: React.ReactNode
}) {
  return (
    <div className="rounded-xl border border-border bg-white shadow-sm">
      <div className="flex items-center gap-2 px-5 py-4 border-b border-border">
        <Icon className="h-4 w-4 text-[#00a65a]" />
        <h3 className="text-sm font-semibold text-gray-700">{title}</h3>
      </div>
      <div className="p-5 space-y-4">{children}</div>
    </div>
  )
}

function SettingRow({ label, children }: { label: string; children: React.ReactNode }) {
  return (
    <div className="flex items-center justify-between">
      <span className="text-sm text-gray-600">{label}</span>
      <div className="w-64">{children}</div>
    </div>
  )
}

function SettingInput({ value, type = 'text' }: { value: string; type?: string }) {
  return (
    <input
      type={type}
      defaultValue={value}
      className="w-full rounded-lg border border-border px-3 py-1.5 text-sm outline-none focus:border-[#00a65a] focus:ring-1 focus:ring-[#00a65a]/20"
    />
  )
}

function SettingToggle({ defaultChecked }: { defaultChecked: boolean }) {
  const [on, setOn] = useState(defaultChecked)
  return (
    <button
      onClick={() => setOn(!on)}
      className={`relative h-6 w-11 rounded-full transition-colors ${on ? 'bg-[#00a65a]' : 'bg-gray-300'}`}
    >
      <span className={`absolute top-0.5 left-0.5 h-5 w-5 rounded-full bg-white shadow transition-transform ${on ? 'translate-x-5' : ''}`} />
    </button>
  )
}

export default function SettingsPage() {
  const { t } = useTranslation('settings')
  const ct = useChineseText()
  const { automations, runAutomation, updateAutomation } = useAutomations()
  const { userRole } = useOffice()

  const typeLabels: Record<string, string> = {
    daily_report: t('workbuddy.autoDailyReport'),
    env_check: t('workbuddy.autoEnvCheck'),
    nutrient_reminder: t('workbuddy.autoNutrientReminder'),
    harvest_reminder: t('workbuddy.autoHarvestReminder'),
    custom: t('workbuddy.autoCustom'),
  }

  return (
    <div className="space-y-6">
      <h2 className="text-xl font-bold">{t('title')}</h2>

      <div className="grid grid-cols-1 gap-6 lg:grid-cols-2">
        {/* Wi-Fi */}
        <SettingsSection title={t('wifi.title')} icon={Wifi}>
          <SettingRow label={t('wifi.ssid')}>
            <SettingInput value="GreenieFarm_5G" />
          </SettingRow>
          <SettingRow label={t('wifi.password')}>
            <SettingInput value="********" type="password" />
          </SettingRow>
          <SettingRow label={t('wifi.autoConnect')}>
            <SettingToggle defaultChecked={true} />
          </SettingRow>
        </SettingsSection>

        {/* AI Model */}
        <SettingsSection title={t('ai.title')} icon={Bot}>
          <SettingRow label={t('ai.model')}>
            <select defaultValue="gpt-4o" className="w-full rounded-lg border border-border px-3 py-1.5 text-sm outline-none focus:border-[#00a65a]">
              <option value="gpt-4o">GPT-4o</option>
              <option value="gpt-4o-mini">GPT-4o Mini</option>
              <option value="claude-3.5">Claude 3.5 Sonnet</option>
            </select>
          </SettingRow>
          <SettingRow label={t('ai.temperature')}>
            <SettingInput value="0.7" />
          </SettingRow>
          <SettingRow label={t('ai.enabled')}>
            <SettingToggle defaultChecked={true} />
          </SettingRow>
        </SettingsSection>

        {/* MQTT */}
        <SettingsSection title={t('mqtt.title')} icon={Radio}>
          <SettingRow label={t('mqtt.host')}>
            <SettingInput value="broker.greeny.local" />
          </SettingRow>
          <SettingRow label={t('mqtt.port')}>
            <SettingInput value="1883" />
          </SettingRow>
          <SettingRow label={t('mqtt.topic')}>
            <SettingInput value="greeny/sensors/#" />
          </SettingRow>
          <SettingRow label={t('mqtt.qos')}>
            <select defaultValue="1" className="w-full rounded-lg border border-border px-3 py-1.5 text-sm outline-none">
              <option value="0">QoS 0 ({ct('最多一次')})</option>
              <option value="1">QoS 1 ({ct('至少一次')})</option>
              <option value="2">QoS 2 ({ct('剛好一次')})</option>
            </select>
          </SettingRow>
        </SettingsSection>

        {/* Notifications */}
        <SettingsSection title={t('notification.title')} icon={Bell}>
          <SettingRow label={t('notification.email')}>
            <SettingToggle defaultChecked={true} />
          </SettingRow>
          <SettingRow label={t('notification.emailAddr')}>
            <SettingInput value="admin@greeny.farm" />
          </SettingRow>
          <SettingRow label={t('notification.push')}>
            <SettingToggle defaultChecked={true} />
          </SettingRow>
          <SettingRow label={t('notification.alert')}>
            <SettingToggle defaultChecked={true} />
          </SettingRow>
        </SettingsSection>

        {/* Maintenance */}
        <SettingsSection title={t('maintenance.title')} icon={Wrench}>
          <div className="space-y-3">
            <div className="flex items-center justify-between">
              <span className="text-sm text-gray-600">{t('maintenance.phRange')}</span>
              <div className="flex items-center gap-2">
                <input defaultValue="5.5" className="w-16 rounded-lg border border-border px-2 py-1 text-sm text-center outline-none focus:border-[#00a65a]" />
                <span className="text-gray-400">~</span>
                <input defaultValue="7.0" className="w-16 rounded-lg border border-border px-2 py-1 text-sm text-center outline-none focus:border-[#00a65a]" />
              </div>
            </div>
            <div className="flex items-center justify-between">
              <span className="text-sm text-gray-600">{t('maintenance.ecMax')}</span>
              <input defaultValue="2000" className="w-20 rounded-lg border border-border px-2 py-1 text-sm text-center outline-none focus:border-[#00a65a]" />
            </div>
            <div className="flex items-center justify-between">
              <span className="text-sm text-gray-600">{t('maintenance.tempRange')}</span>
              <div className="flex items-center gap-2">
                <input defaultValue="18" className="w-16 rounded-lg border border-border px-2 py-1 text-sm text-center outline-none focus:border-[#00a65a]" />
                <span className="text-gray-400">~</span>
                <input defaultValue="30" className="w-16 rounded-lg border border-border px-2 py-1 text-sm text-center outline-none focus:border-[#00a65a]" />
              </div>
            </div>
            <div className="flex items-center justify-between">
              <span className="text-sm text-gray-600">{t('maintenance.doMin')}</span>
              <input defaultValue="6.0" className="w-20 rounded-lg border border-border px-2 py-1 text-sm text-center outline-none focus:border-[#00a65a]" />
            </div>
          </div>
          <div className="pt-3 border-t border-border flex gap-2">
            <button className="rounded-lg bg-[#00a65a] px-4 py-2 text-sm text-white hover:bg-[#00954f] transition-colors">
              {t('maintenance.save')}
            </button>
            <button className="rounded-lg border border-border px-4 py-2 text-sm text-gray-600 hover:bg-gray-50 transition-colors">
              {t('maintenance.reset')}
            </button>
          </div>
        </SettingsSection>

        {/* System Info */}
        <SettingsSection title={t('system.title')} icon={Info}>
          <div className="space-y-3 text-sm">
            <div className="flex items-center justify-between">
              <span className="text-gray-500">{t('system.systemName')}</span>
              <span className="text-gray-700 font-medium">Greenie {t('common:brand.subtitle')}</span>
            </div>
            <div className="flex items-center justify-between">
              <span className="text-gray-500">{t('system.version')}</span>
              <span className="text-gray-700">v2.0.1</span>
            </div>
            <div className="flex items-center justify-between">
              <span className="text-gray-500">{t('system.firmware')}</span>
              <span className="text-gray-700">v1.3.7</span>
            </div>
            <div className="flex items-center justify-between">
              <span className="text-gray-500">{t('system.uptime')}</span>
              <span className="text-gray-700">{ct('15 天 8 小時')}</span>
            </div>
            <div className="flex items-center justify-between">
              <span className="text-gray-500">{t('system.dbSize')}</span>
              <span className="text-gray-700">12.4 MB</span>
            </div>
            <div className="flex items-center justify-between">
              <span className="text-gray-500">{t('system.connectedDevices')}</span>
              <span className="text-gray-700">15 / 20</span>
            </div>
            <div className="flex items-center justify-between">
              <span className="text-gray-500">{t('system.lastSync')}</span>
              <span className="text-gray-700">{ct('剛剛')}</span>
            </div>
          </div>
          <div className="pt-3 border-t border-border">
            <button className="rounded-lg border border-border px-4 py-2 text-sm text-gray-600 hover:bg-gray-50 transition-colors">
              {t('system.checkUpdate')}
            </button>
          </div>
        </SettingsSection>

        {/* WorkBuddy */}
        <SettingsSection title={t('workbuddy.title')} icon={Zap}>
          <SettingRow label={t('workbuddy.apiUrl')}><SettingInput value="" /></SettingRow>
          <SettingRow label={t('workbuddy.apiKey')}><SettingInput value="" type="password" /></SettingRow>
          <SettingRow label={t('workbuddy.whatsappEnabled')}><SettingToggle defaultChecked={false} /></SettingRow>
          <SettingRow label={t('workbuddy.notifyCritical')}><SettingToggle defaultChecked={true} /></SettingRow>
          <SettingRow label={t('workbuddy.notifyWarning')}><SettingToggle defaultChecked={false} /></SettingRow>
        </SettingsSection>
      </div>

      {/* User Management */}
      {(userRole === 'superadmin' || userRole === 'office_admin') && (
        <UserManagementSection />
      )}

      {/* Automations */}
      <div className="rounded-xl border border-border bg-white shadow-sm">
        <div className="flex items-center gap-2 px-5 py-4 border-b border-border">
          <Zap className="h-4 w-4 text-[#00a65a]" />
          <h3 className="text-sm font-semibold text-gray-700">{t('workbuddy.automations')}</h3>
        </div>
        <div className="divide-y divide-border">
          {automations.length === 0 ? (
            <div className="px-5 py-8 text-center text-gray-400 text-sm">{t('workbuddy.noAutomations')}</div>
          ) : automations.map((auto) => (
            <div key={auto.id} className="flex items-center justify-between px-5 py-3 hover:bg-gray-50">
              <div className="flex items-center gap-3">
                <button onClick={() => updateAutomation(auto.id, { enabled: auto.enabled ? 0 : 1 })}
                  className={`relative h-5 w-9 rounded-full transition-colors ${auto.enabled ? 'bg-[#00a65a]' : 'bg-gray-300'}`}>
                  <span className={`absolute top-0.5 left-0.5 h-4 w-4 rounded-full bg-white shadow transition-transform ${auto.enabled ? 'translate-x-4' : ''}`} />
                </button>
                <div>
                  <p className="text-sm font-medium text-gray-800">{auto.name}</p>
                  <p className="text-[10px] text-gray-400">{typeLabels[auto.type] || auto.type} · {auto.cron_expr}</p>
                </div>
              </div>
              <div className="flex items-center gap-2">
                <span className="text-[10px] text-gray-400">{t('workbuddy.lastRun')}: {auto.last_run_at ? timeAgo(auto.last_run_at) : t('workbuddy.never')}</span>
                <button onClick={() => runAutomation(auto.id)}
                  className="rounded-lg border border-[#00a65a]/30 bg-[#e8f5e9] px-2 py-1 text-[10px] text-[#2E7D32] hover:bg-[#c8e6c9]">
                  {t('common:actions.run')}
                </button>
              </div>
            </div>
          ))}
        </div>
      </div>
    </div>
  )
}

function UserManagementSection() {
  const { t } = useTranslation('settings')
  const { userRole } = useOffice()
  const [users, setUsers] = useState<any[]>([])
  const [loading, setLoading] = useState(true)
  const [showAdd, setShowAdd] = useState(false)
  const [form, setForm] = useState({ username: '', password: '', role: 'staff', display_name: '', office_id: '' })

  const fetchUsers = useCallback(async () => {
    try {
      const data = await apiFetch<any[]>('/users')
      setUsers(data)
    } catch { /* */ } finally { setLoading(false) }
  }, [])

  useEffect(() => { fetchUsers() }, [fetchUsers])

  const handleAdd = async (e: React.FormEvent) => {
    e.preventDefault()
    await apiFetch('/users', {
      method: 'POST',
      body: JSON.stringify({
        ...form,
        office_id: form.office_id ? Number(form.office_id) : undefined,
      }),
    })
    setShowAdd(false)
    setForm({ username: '', password: '', role: 'staff', display_name: '', office_id: '' })
    fetchUsers()
  }

  const handleDelete = async (id: number) => {
    if (!confirm('Deactivate this user?')) return
    await apiFetch(`/users/${id}`, { method: 'DELETE' })
    fetchUsers()
  }

  const roleLabels: Record<string, string> = {
    superadmin: 'Super Admin',
    office_admin: 'Office Admin',
    staff: 'Staff',
  }

  return (
    <div className="rounded-xl border border-border bg-white shadow-sm">
      <div className="flex items-center justify-between px-5 py-4 border-b border-border">
        <div className="flex items-center gap-2">
          <Users className="h-4 w-4 text-[#00a65a]" />
          <h3 className="text-sm font-semibold text-gray-700">{t('users.title', { defaultValue: 'User Management' })}</h3>
        </div>
        <button onClick={() => setShowAdd(!showAdd)}
          className="flex items-center gap-1 rounded-lg bg-[#00a65a] px-2 py-1 text-xs text-white hover:bg-[#00954f]">
          <Plus className="h-3 w-3" />{t('users.add', { defaultValue: 'Add User' })}
        </button>
      </div>

      {showAdd && (
        <form onSubmit={handleAdd} className="border-b border-border bg-gray-50 px-5 py-3 flex items-end gap-3">
          <div>
            <label className="block text-[10px] text-gray-500">Username</label>
            <input value={form.username} onChange={(e) => setForm({ ...form, username: e.target.value })}
              className="rounded border border-gray-300 px-2 py-1 text-sm w-28" required />
          </div>
          <div>
            <label className="block text-[10px] text-gray-500">Password</label>
            <input type="password" value={form.password} onChange={(e) => setForm({ ...form, password: e.target.value })}
              className="rounded border border-gray-300 px-2 py-1 text-sm w-28" required />
          </div>
          <div>
            <label className="block text-[10px] text-gray-500">Display Name</label>
            <input value={form.display_name} onChange={(e) => setForm({ ...form, display_name: e.target.value })}
              className="rounded border border-gray-300 px-2 py-1 text-sm w-28" />
          </div>
          {userRole === 'superadmin' && (
            <>
              <div>
                <label className="block text-[10px] text-gray-500">Role</label>
                <select value={form.role} onChange={(e) => setForm({ ...form, role: e.target.value })}
                  className="rounded border border-gray-300 px-2 py-1 text-sm">
                  <option value="staff">Staff</option>
                  <option value="office_admin">Office Admin</option>
                  <option value="superadmin">Super Admin</option>
                </select>
              </div>
              <div>
                <label className="block text-[10px] text-gray-500">Office ID</label>
                <input type="number" value={form.office_id} onChange={(e) => setForm({ ...form, office_id: e.target.value })}
                  className="rounded border border-gray-300 px-2 py-1 text-sm w-20" />
              </div>
            </>
          )}
          <button type="submit" className="rounded bg-[#00a65a] px-3 py-1 text-sm text-white">Add</button>
        </form>
      )}

      <div className="divide-y divide-border">
        {loading ? (
          <div className="px-5 py-8 text-center text-gray-400 text-sm">{t('common:actions.loading')}</div>
        ) : users.length === 0 ? (
          <div className="px-5 py-8 text-center text-gray-400 text-sm">No users found</div>
        ) : users.map((u) => (
          <div key={u.id} className="flex items-center justify-between px-5 py-3 hover:bg-gray-50">
            <div className="flex items-center gap-3">
              <div className="flex h-8 w-8 items-center justify-center rounded-full bg-gray-100 text-xs font-medium text-gray-600">
                {(u.display_name || u.username).charAt(0).toUpperCase()}
              </div>
              <div>
                <p className="text-sm font-medium text-gray-800">{u.display_name || u.username}</p>
                <p className="text-[10px] text-gray-400">{u.username} · Office #{u.office_id || 'All'}</p>
              </div>
            </div>
            <div className="flex items-center gap-2">
              <span className={`rounded-full px-2 py-0.5 text-[10px] font-medium ${
                u.role === 'superadmin' ? 'bg-purple-100 text-purple-700' :
                u.role === 'office_admin' ? 'bg-blue-100 text-blue-700' :
                'bg-gray-100 text-gray-700'
              }`}>{roleLabels[u.role] || u.role}</span>
              {!u.active && <span className="rounded-full bg-red-100 px-2 py-0.5 text-[10px] text-red-600">Inactive</span>}
              {u.active && <button onClick={() => handleDelete(u.id)} className="rounded p-1 text-gray-400 hover:text-red-500"><Trash2 className="h-3 w-3" /></button>}
            </div>
          </div>
        ))}
      </div>
    </div>
  )
}
