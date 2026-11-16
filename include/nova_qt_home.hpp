#pragma once
#include <QObject>
#include <QVariantList>
#include <QString>

// Qt Quick presentation adapter; it reads the existing C++ service API.
// No playback, authentication or iPhone controller code is rewritten.
class NovaQtHome : public QObject {
    Q_OBJECT
    Q_PROPERTY(QVariantList channels READ channels NOTIFY changed)
    Q_PROPERTY(QVariantList series READ series NOTIFY changed)
    Q_PROPERTY(QVariantList movies READ movies NOTIFY changed)
    Q_PROPERTY(QString status READ status NOTIFY changed)
    Q_PROPERTY(bool loading READ loading NOTIFY changed)
public:
    explicit NovaQtHome(QObject* parent=nullptr);
    const QVariantList& channels() const {return channels_;}
    const QVariantList& series() const {return series_;}
    const QVariantList& movies() const {return movies_;}
    QString status() const {return status_;}
    bool loading() const {return loading_;}
    Q_INVOKABLE void refresh();

signals:
    void changed();

private:
    QVariantList channels_,series_,movies_;
    QString status_=QStringLiteral("Preparando catálogo...");
    bool loading_=false;
};
