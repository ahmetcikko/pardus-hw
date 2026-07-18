#pragma once
#include <QObject>
#include <QString>
#include <QTimer>
#include <QTranslator>
#include <QVariant>
#include <cstdint>
#include <string>
#include <unordered_map>
#include <utility>

class QQmlEngine;
class Backend : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString cpuText READ cpuText NOTIFY statsChanged)
    Q_PROPERTY(QString ramText READ ramText NOTIFY statsChanged)
    Q_PROPERTY(QString diskText READ diskText NOTIFY statsChanged)
    Q_PROPERTY(QString gpuText READ gpuText NOTIFY statsChanged)
    Q_PROPERTY(QString netText READ netText NOTIFY statsChanged)
    Q_PROPERTY(QVariantList heavyTasks READ heavyTasks NOTIFY statsChanged)
  public:
    explicit Backend(QObject *parent = nullptr);
    void set_engine(QQmlEngine *engine);
    QString cpuText() const;
    QString ramText() const;
    QString diskText() const;
    QString gpuText() const;
    QString netText() const;
    QVariantList heavyTasks() const;
    Q_INVOKABLE QVariantList details(const QString &kind);
    Q_INVOKABLE bool killProcess(int pid);
    Q_INVOKABLE bool throttleProcess(int pid);
    Q_INVOKABLE bool revealProcess(int pid);
    Q_INVOKABLE void startOptimize();
    Q_INVOKABLE void setLanguage(const QString &lang);
  signals:
    void statsChanged();
    void optimizeDone(bool ok, int freedMb);

  private:
    void prime();
    void poll();
    QVariantList cpu_details();
    QVariantList ram_details();
    QVariantList disk_details();
    QVariantList gpu_details();
    QVariantList net_details();
    QTimer m_timer;
    long m_ncpu;
    std::uint64_t m_prevtotal;
    std::uint64_t m_previdle;
    std::unordered_map<std::string, std::pair<std::uint64_t, std::uint64_t>>
        m_prevnet;
    std::unordered_map<std::string, std::pair<double, double>> m_ifrates;
    QString m_cputext;
    QString m_ramtext;
    QString m_disktext;
    QString m_gputext;
    QString m_nettext;
    QVariantList m_heavytasks;
    QTranslator m_translator;
    QQmlEngine *m_engine;
    std::uint64_t m_optbefore;
};
