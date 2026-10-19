// System facts and system-level changes for chrono-settings: time & date, keyboard layouts,
// cursor/icon themes, sound devices, autostart, and the About page.
#pragma once
#include <QObject>
#include <QVariant>

class Backend : public QObject
{
	Q_OBJECT
	Q_PROPERTY(bool hasTimedatectl READ hasTimedatectl CONSTANT)
public:
	using QObject::QObject;
	bool hasTimedatectl() const;

	// time & date
	Q_INVOKABLE QStringList timezones() const;
	Q_INVOKABLE QString timezone() const;
	Q_INVOKABLE bool ntp() const;
	Q_INVOKABLE void setTimezone(const QString &tz);
	Q_INVOKABLE void setNtp(bool on);
	Q_INVOKABLE void setDateTime(const QString &isoLocal); // "2026-10-18 17:05:00"

	// keyboard
	Q_INVOKABLE QVariantList keyboardLayouts() const;               // [{id, name}]
	Q_INVOKABLE QVariantList keyboardVariants(const QString &layout) const;
	Q_INVOKABLE QVariantList keyboardModels() const;

	// appearance extras
	Q_INVOKABLE QStringList cursorThemes() const;
	Q_INVOKABLE QStringList iconThemes() const;

	// sound (PipeWire / PulseAudio via pactl)
	Q_INVOKABLE QVariantList sinks() const;                          // [{name, description, default}]
	Q_INVOKABLE QVariantList sources() const;
	Q_INVOKABLE void setDefaultSink(const QString &name);
	Q_INVOKABLE void setDefaultSource(const QString &name);

	// apps
	Q_INVOKABLE QVariantList autostart() const;                      // [{file, name, enabled, user}]
	Q_INVOKABLE void setAutostart(const QString &file, bool enabled);

	// about
	Q_INVOKABLE QVariantMap systemInfo() const;
	Q_INVOKABLE bool has(const QString &program) const;
	Q_INVOKABLE void run(const QString &cmd) const;

signals:
	void timeChanged();
	void soundChanged();
};
