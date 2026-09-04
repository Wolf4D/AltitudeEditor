#include "LanguageManager.h"
#include <QCoreApplication>
#include <QApplication>
#include <QWidget>
#include <QSettings>
#include <QEvent>
#include <QDebug>

LanguageManager& LanguageManager::instance() {
    static LanguageManager s_instance;
    return s_instance;
}

LanguageManager::LanguageManager(QObject* parent)
    : QObject(parent)
{
}

void LanguageManager::init() {
    QSettings settings(QStringLiteral("Madness Studio"), QStringLiteral("Altitude Editor"));
    QString langStr = settings.value(QStringLiteral("Language"), QStringLiteral("auto")).toString().toLower();

    Language lang = Language::Auto;
    if (langStr == QStringLiteral("ru") || langStr == QStringLiteral("russian")) {
        lang = Language::Russian;
    } else if (langStr == QStringLiteral("en") || langStr == QStringLiteral("english")) {
        lang = Language::English;
    } else {
        lang = Language::Auto;
    }

    applyLanguage(lang);
}

void LanguageManager::setLanguage(Language lang) {
    if (m_currentLanguage == lang) return;

    QSettings settings(QStringLiteral("Madness Studio"), QStringLiteral("Altitude Editor"));
    if (lang == Language::Auto) {
        settings.setValue(QStringLiteral("Language"), QStringLiteral("auto"));
    } else if (lang == Language::Russian) {
        settings.setValue(QStringLiteral("Language"), QStringLiteral("ru"));
    } else {
        settings.setValue(QStringLiteral("Language"), QStringLiteral("en"));
    }

    applyLanguage(lang);
}

QString LanguageManager::languageCode(Language lang) const {
    switch (lang) {
    case Language::Russian: return QStringLiteral("ru");
    case Language::English: return QStringLiteral("en");
    case Language::Auto:
    default:
        return QStringLiteral("auto");
    }
}

void LanguageManager::applyLanguage(Language lang) {
    m_currentLanguage = lang;

    // Determine effective language
    if (lang == Language::Auto) {
        QLocale sys = QLocale::system();
        if (sys.language() == QLocale::Russian ||
            sys.language() == QLocale::Belarusian ||
            sys.language() == QLocale::Ukrainian ||
            sys.name().startsWith(QStringLiteral("ru"), Qt::CaseInsensitive)) {
            m_effectiveLanguage = Language::Russian;
        } else {
            m_effectiveLanguage = Language::English;
        }
    } else {
        m_effectiveLanguage = lang;
    }

    // Remove existing translators
    qApp->removeTranslator(&m_appTranslator);
    qApp->removeTranslator(&m_qtTranslator);

    if (m_effectiveLanguage == Language::Russian) {
        if (m_appTranslator.load(QStringLiteral(":/translations/altitude_editor_ru.qm"))) {
            qApp->installTranslator(&m_appTranslator);
        } else if (m_appTranslator.load(QStringLiteral("translations/altitude_editor_ru.qm"))) {
            qApp->installTranslator(&m_appTranslator);
        }

        // Try Qt standard translations for common dialog buttons (OK, Cancel, Open, etc.)
        if (m_qtTranslator.load(QStringLiteral(":/translations/qt_ru.qm"))) {
            qApp->installTranslator(&m_qtTranslator);
        }
    } else {
        if (m_appTranslator.load(QStringLiteral(":/translations/altitude_editor_en.qm"))) {
            qApp->installTranslator(&m_appTranslator);
        }
    }

    emit languageChanged();

    // Trigger LanguageChange event on all active top-level widgets
    for (QWidget* w : QApplication::topLevelWidgets()) {
        QEvent ev(QEvent::LanguageChange);
        QApplication::sendEvent(w, &ev);
    }
}
