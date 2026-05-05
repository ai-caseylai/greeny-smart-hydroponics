import { Bell, ChevronDown, Menu } from 'lucide-react'
import { useTranslation } from 'react-i18next'
import { useAuth } from '../../hooks/useAuth'
import { useNavigate } from 'react-router-dom'
import { useOffice } from '../../context/OfficeContext'
import { useOffices } from '../../hooks/useOffices'
import { useSidebar } from './DashboardLayout'
import { LanguageSwitcher } from './LanguageSwitcher'

export function Header() {
  const { user, logout } = useAuth()
  const { t } = useTranslation()
  const navigate = useNavigate()
  const { selectedOfficeId, setSelectedOfficeId, lockedOfficeId } = useOffice()
  const { offices } = useOffices()
  const { setOpen } = useSidebar()

  const showOfficeSelector = lockedOfficeId === undefined || lockedOfficeId === null

  return (
    <header className="sticky top-0 z-30 flex h-14 items-center justify-between border-b border-border bg-white px-4 lg:px-6">
      <div className="flex items-center gap-3">
        <button
          onClick={() => setOpen(true)}
          className="lg:hidden rounded-lg p-2 hover:bg-gray-100 transition-colors"
        >
          <Menu className="h-5 w-5 text-gray-600" />
        </button>
      </div>
      <div className="flex items-center gap-2 sm:gap-4">
        {showOfficeSelector && (
          <div className="relative hidden sm:block">
            <select
              value={selectedOfficeId ?? ''}
              onChange={(e) => setSelectedOfficeId(e.target.value ? Number(e.target.value) : null)}
              className="appearance-none rounded-lg border border-gray-200 bg-gray-50 px-3 py-1.5 pr-8 text-sm outline-none hover:border-green-400 cursor-pointer"
            >
              <option value="">{t('racks:allOffices')}</option>
              {offices.map((o) => (
                <option key={o.id} value={o.id}>{o.name}</option>
              ))}
            </select>
            <ChevronDown className="pointer-events-none absolute right-2 top-2 h-4 w-4 text-gray-400" />
          </div>
        )}
        <LanguageSwitcher />
        <button
          onClick={() => navigate('/alerts')}
          className="relative rounded-lg p-2 hover:bg-gray-100 transition-colors"
        >
          <Bell className="h-5 w-5 text-gray-600" />
        </button>
        <div className="flex items-center gap-2 text-sm">
          <div className="flex h-8 w-8 items-center justify-center rounded-full bg-[#e8f5e9] text-[#1B5E20] text-xs font-bold">
            {(user?.display_name || user?.username || 'A')[0]}
          </div>
          <span className="text-gray-700 hidden sm:inline">{user?.display_name || user?.username}</span>
          <span className="text-[10px] text-gray-400 hidden md:inline">{user?.role === 'superadmin' ? 'SA' : user?.role === 'office_admin' ? 'OA' : 'ST'}</span>
        </div>
        <button
          onClick={logout}
          className="rounded-lg px-3 py-1.5 text-sm text-gray-500 hover:bg-gray-100 hover:text-gray-700 transition-colors"
        >
          <span className="hidden sm:inline">{t('actions.logout')}</span>
          <span className="sm:hidden text-xs">登出</span>
        </button>
      </div>
    </header>
  )
}
