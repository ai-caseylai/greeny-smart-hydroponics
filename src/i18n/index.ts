import i18n from 'i18next'
import { initReactI18next } from 'react-i18next'
import LanguageDetector from 'i18next-browser-languagedetector'

import zhTWCommon from './locales/zh-TW/common.json'
import zhTWAuth from './locales/zh-TW/auth.json'
import zhTWDashboard from './locales/zh-TW/dashboard.json'
import zhTWWater from './locales/zh-TW/water.json'
import zhTWDevices from './locales/zh-TW/devices.json'
import zhTWRacks from './locales/zh-TW/racks.json'
import zhTWAlerts from './locales/zh-TW/alerts.json'
import zhTWChat from './locales/zh-TW/chat.json'
import zhTWSettings from './locales/zh-TW/settings.json'

import zhCNCommon from './locales/zh-CN/common.json'
import zhCNAuth from './locales/zh-CN/auth.json'
import zhCNDashboard from './locales/zh-CN/dashboard.json'
import zhCNWater from './locales/zh-CN/water.json'
import zhCNDevices from './locales/zh-CN/devices.json'
import zhCNRacks from './locales/zh-CN/racks.json'
import zhCNAlerts from './locales/zh-CN/alerts.json'
import zhCNChat from './locales/zh-CN/chat.json'
import zhCNSettings from './locales/zh-CN/settings.json'

import enCommon from './locales/en/common.json'
import enAuth from './locales/en/auth.json'
import enDashboard from './locales/en/dashboard.json'
import enWater from './locales/en/water.json'
import enDevices from './locales/en/devices.json'
import enRacks from './locales/en/racks.json'
import enAlerts from './locales/en/alerts.json'
import enChat from './locales/en/chat.json'
import enSettings from './locales/en/settings.json'

const ns = ['common', 'auth', 'dashboard', 'water', 'devices', 'racks', 'alerts', 'chat', 'settings']

i18n
  .use(LanguageDetector)
  .use(initReactI18next)
  .init({
    fallbackLng: 'zh-TW',
    supportedLngs: ['zh-TW', 'zh-CN', 'en'],
    ns,
    defaultNS: 'common',
    resources: {
      'zh-TW': {
        common: zhTWCommon, auth: zhTWAuth, dashboard: zhTWDashboard,
        water: zhTWWater, devices: zhTWDevices, racks: zhTWRacks,
        alerts: zhTWAlerts, chat: zhTWChat, settings: zhTWSettings,
      },
      'zh-CN': {
        common: zhCNCommon, auth: zhCNAuth, dashboard: zhCNDashboard,
        water: zhCNWater, devices: zhCNDevices, racks: zhCNRacks,
        alerts: zhCNAlerts, chat: zhCNChat, settings: zhCNSettings,
      },
      en: {
        common: enCommon, auth: enAuth, dashboard: enDashboard,
        water: enWater, devices: enDevices, racks: enRacks,
        alerts: enAlerts, chat: enChat, settings: enSettings,
      },
    },
    interpolation: { escapeValue: false },
    detection: {
      order: ['localStorage', 'navigator'],
      lookupLocalStorage: 'greeny-language',
      caches: ['localStorage'],
    },
  })

export default i18n
