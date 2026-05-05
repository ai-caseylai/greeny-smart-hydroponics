import { Bell } from 'lucide-react'
import { useTranslation } from 'react-i18next'
import { useAuth } from '../../hooks/useAuth'
import { useNavigate } from 'react-router-dom'
import { LanguageSwitcher } from './LanguageSwitcher'

export function Header() {
  const { user, logout } = useAuth()
  const { t } = useTranslation()
  const navigate = useNavigate()

  return (
    <header className="sticky top-0 z-30 flex h-14 items-center justify-between border-b border-border bg-white px-6">
      <h1 className="text-lg font-semibold" />
      <div className="flex items-center gap-4">
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
          <span className="text-gray-700">{user?.display_name || user?.username}</span>
          <span className="text-[10px] text-gray-400">{user?.role === 'superadmin' ? 'SA' : user?.role === 'office_admin' ? 'OA' : 'ST'}</span>
        </div>
        <button
          onClick={logout}
          className="rounded-lg px-3 py-1.5 text-sm text-gray-500 hover:bg-gray-100 hover:text-gray-700 transition-colors"
        >
          {t('actions.logout')}
        </button>
      </div>
    </header>
  )
}
