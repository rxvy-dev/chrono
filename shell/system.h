// Status for the panel: volume, brightness, battery, network, media; plus power actions.
// Uses standard tools (wpctl, brightnessctl, playerctl, nmcli, systemctl) when they're installed.
#pragma once
#include <QObject>
#include <QProcess>
#include <QTimer>
#include <QVariant>

class System : public QObject
{
	Q_OBJECT
	Q_PROPERTY(int volume READ volume NOTIFY audioChanged)
	Q_PROPERTY(bool muted READ muted NOTIFY audioChanged)
	Q_PROPERTY(bool micMuted READ micMuted NOTIFY audioChanged)
	Q_PROPERTY(bool hasAudio READ hasAudio NOTIFY audioChanged)
	Q_PROPERTY(int brightness READ brightness NOTIFY brightnessChanged)
	Q_PROPERTY(bool hasBacklight READ hasBacklight CONSTANT)
	Q_PROPERTY(bool hasBattery READ hasBattery NOTIFY batteryChanged)
	Q_PROPERTY(int battery READ battery NOTIFY batteryChanged)
	Q_PROPERTY(bool charging READ charging NOTIFY batteryChanged)
	Q_PROPERTY(QString network READ network NOTIFY networkChanged)       // "wifi" | "ethernet" | "none"
	Q_PROPERTY(QString networkName READ networkName NOTIFY networkChanged)
	Q_PROPERTY(int wifiStrength READ wifiStrength NOTIFY networkChanged)
	Q_PROPERTY(QString mediaTitle READ mediaTitle NOTIFY mediaChanged)
	Q_PROPERTY(QString mediaArtist READ mediaArtist NOTIFY mediaChanged)
	Q_PROPERTY(bool mediaPlaying READ mediaPlaying NOTIFY mediaChanged)
	Q_PROPERTY(QString userName READ userName CONSTANT)
	Q_PROPERTY(QString hostName READ hostName CONSTANT)
public:
	explicit System(QObject *parent = nullptr);
	int volume() const { return m_volume; }
	bool muted() const { return m_muted; }
	bool micMuted() const { return m_micMuted; }
	bool hasAudio() const { return m_hasAudio; }
	int brightness() const { return m_brightness; }
	bool hasBacklight() const { return !m_backlight.isEmpty(); }
	bool hasBattery() const { return !m_battery.isEmpty(); }
	int battery() const { return m_batteryPct; }
	bool charging() const { return m_charging; }
	QString network() const { return m_net; }
	QString networkName() const { return m_netName; }
	int wifiStrength() const { return m_wifi; }
	QString mediaTitle() const { return m_mediaTitle; }
	QString mediaArtist() const { return m_mediaArtist; }
	bool mediaPlaying() const { return m_mediaPlaying; }
	QString userName() const;
	QString hostName() const;

	Q_INVOKABLE void setVolume(int pct);
	Q_INVOKABLE void toggleMute();
	Q_INVOKABLE void setBrightness(int pct);
	Q_INVOKABLE void media(const QString &cmd); // play-pause | next | previous
	Q_INVOKABLE void power(const QString &what); // shutdown | reboot | suspend | hibernate
	Q_INVOKABLE bool has(const QString &program) const;
	Q_INVOKABLE void refreshAudio();
	Q_INVOKABLE void refreshBrightness();
	Q_INVOKABLE QVariantList wifiNetworks();   // [{ssid, signal, secure, active}]
	Q_INVOKABLE void connectWifi(const QString &ssid);

signals:
	void audioChanged();
	void brightnessChanged();
	void batteryChanged();
	void networkChanged();
	void mediaChanged();

private:
	void pollSlow();
	void refreshBattery();
	void refreshNetwork();
	void refreshMedia();
	void run(const QString &prog, const QStringList &args, std::function<void(const QString &)> done);

	int m_volume = 0;
	bool m_muted = false, m_micMuted = false, m_hasAudio = false;
	int m_brightness = 100;
	QString m_backlight, m_battery;
	int m_batteryPct = 100;
	bool m_charging = false;
	QString m_net = QStringLiteral("none"), m_netName;
	int m_wifi = 0;
	QString m_mediaTitle, m_mediaArtist;
	bool m_mediaPlaying = false;
	QTimer m_fast, m_slow;
};
