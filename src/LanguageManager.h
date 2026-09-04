#pragma once

#include <QObject>
#include <QString>
#include <QTranslator>
#include <QLocale>

class LanguageManager : public QObject {
    Q_OBJECT

public:
    enum class Language {
        Auto,
        English,
        Russian
    };

    static LanguageManager& instance();

    void init();

    Language currentLanguage() const { return m_currentLanguage; }
    Language effectiveLanguage() const { return m_effectiveLanguage; }

    void setLanguage(Language lang);
    QString languageCode(Language lang) const;

signals:
    void languageChanged();

private:
    LanguageManager(QObject* parent = nullptr);
    ~LanguageManager() override = default;
    LanguageManager(const LanguageManager&) = delete;
    LanguageManager& operator=(const LanguageManager&) = delete;

    void applyLanguage(Language lang);

    Language m_currentLanguage = Language::Auto;
    Language m_effectiveLanguage = Language::English;
    QTranslator m_appTranslator;
    QTranslator m_qtTranslator;
};
