import { useTranslation } from 'react-i18next'
import { Globe } from 'lucide-react'

const languages = [
  { code: 'zh-TW', label: '繁體' },
  { code: 'zh-CN', label: '简体' },
  { code: 'en', label: 'EN' },
]

export function LanguageSwitcher() {
  const { i18n } = useTranslation()

  return (
    <div className="flex items-center gap-1 rounded-lg border border-border px-1 py-0.5">
      <Globe className="h-3.5 w-3.5 text-gray-400 ml-1" />
      {languages.map((lang) => (
        <button
          key={lang.code}
          onClick={() => i18n.changeLanguage(lang.code)}
          className={`rounded px-1.5 py-0.5 text-[11px] transition-colors ${
            i18n.language === lang.code || (i18n.language?.startsWith(lang.code.split('-')[0]) && lang.code === 'zh-TW' && i18n.language === 'zh-TW')
              ? 'bg-[#00a65a] text-white font-medium'
              : 'text-gray-500 hover:text-gray-700'
          }`}
        >
          {lang.label}
        </button>
      ))}
    </div>
  )
}
