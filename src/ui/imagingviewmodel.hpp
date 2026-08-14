#pragma once

#include <QPointer>
#include <QString>
#include <QUrl>

#include <imaging/imagingtypes.hpp>
#include <ui/viewmodel.hpp>


namespace UDI
{
class DeviceListModel;

// Drives the Write, Read and Verify pages: the chosen image and options, the live progress of a run, and
// its outcome. Every value QML binds to is pre-formatted text, so a binding never does arithmetic.
class ImagingViewModel : public IViewModel
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(ImagingViewModel)

    Q_PROPERTY(QString imageFilePath READ getImageFilePath WRITE setImageFilePath NOTIFY imageFileChanged FINAL)
    Q_PROPERTY(QString imageFileName READ getImageFileName NOTIFY imageFileChanged FINAL)
    Q_PROPERTY(QString imageSizeText READ getImageSizeText NOTIFY imageFileChanged FINAL)
    Q_PROPERTY(bool imageFileExists READ getImageFileExists NOTIFY imageFileChanged FINAL)

    Q_PROPERTY(QString trimMode READ getTrimMode WRITE setTrimMode NOTIFY optionsChanged FINAL)
    Q_PROPERTY(bool verifyAfterWrite READ getVerifyAfterWrite WRITE setVerifyAfterWrite NOTIFY optionsChanged FINAL)
    Q_PROPERTY(bool computeSha256 READ getComputeSha256 WRITE setComputeSha256 NOTIFY optionsChanged FINAL)

    Q_PROPERTY(bool busy READ getBusy NOTIFY stateChanged FINAL)
    Q_PROPERTY(bool cancelling READ getCancelling NOTIFY stateChanged FINAL)
    Q_PROPERTY(QString operation READ getOperationToken NOTIFY stateChanged FINAL)

    Q_PROPERTY(double progressFraction READ getProgressFraction NOTIFY progressChanged FINAL)
    Q_PROPERTY(QString progressPercentText READ getProgressPercentText NOTIFY progressChanged FINAL)
    Q_PROPERTY(QString stageText READ getStageText NOTIFY progressChanged FINAL)
    Q_PROPERTY(QString processedSizeText READ getProcessedSizeText NOTIFY progressChanged FINAL)
    Q_PROPERTY(QString totalSizeText READ getTotalSizeText NOTIFY progressChanged FINAL)
    Q_PROPERTY(QString transferRateText READ getTransferRateText NOTIFY progressChanged FINAL)
    Q_PROPERTY(QString remainingTimeText READ getRemainingTimeText NOTIFY progressChanged FINAL)
    Q_PROPERTY(QString elapsedTimeText READ getElapsedTimeText NOTIFY progressChanged FINAL)

    Q_PROPERTY(bool hasResult READ getHasResult NOTIFY resultChanged FINAL)
    Q_PROPERTY(bool resultSucceeded READ getResultSucceeded NOTIFY resultChanged FINAL)
    Q_PROPERTY(bool resultCancelled READ getResultCancelled NOTIFY resultChanged FINAL)
    Q_PROPERTY(bool resultVerified READ getResultVerified NOTIFY resultChanged FINAL)
    Q_PROPERTY(QString resultTitle READ getResultTitle NOTIFY resultChanged FINAL)
    Q_PROPERTY(QString resultMessage READ getResultMessage NOTIFY resultChanged FINAL)
    Q_PROPERTY(QString resultFastDigest READ getResultFastDigest NOTIFY resultChanged FINAL)
    Q_PROPERTY(QString resultSha256Digest READ getResultSha256Digest NOTIFY resultChanged FINAL)

public:
    explicit ImagingViewModel(DeviceListModel* deviceListModel);

    void resetToDefault() override;
    void onLanguageChanged() override;

    QString getImageFilePath() const { return m_imageFilePath; }
    void setImageFilePath(const QString& imageFilePath);
    QString getImageFileName() const;
    QString getImageSizeText() const;
    bool getImageFileExists() const;

    QString getTrimMode() const { return m_trimMode; }
    void setTrimMode(const QString& trimMode);
    bool getVerifyAfterWrite() const { return m_verifyAfterWrite; }
    void setVerifyAfterWrite(bool verifyAfterWrite);
    bool getComputeSha256() const { return m_computeSha256; }
    void setComputeSha256(bool computeSha256);

    bool getBusy() const { return m_busy; }
    bool getCancelling() const { return m_cancelling; }
    QString getOperationToken() const;

    double getProgressFraction() const;
    QString getProgressPercentText() const;
    QString getStageText() const;
    QString getProcessedSizeText() const;
    QString getTotalSizeText() const;
    QString getTransferRateText() const;
    QString getRemainingTimeText() const;
    QString getElapsedTimeText() const;

    bool getHasResult() const { return m_hasResult; }
    bool getResultSucceeded() const { return m_result.succeeded; }
    bool getResultCancelled() const { return m_result.cancelled; }
    bool getResultVerified() const { return m_result.verificationMatched; }
    QString getResultTitle() const;
    QString getResultMessage() const;
    QString getResultFastDigest() const;
    QString getResultSha256Digest() const;

    Q_INVOKABLE void startRead();
    Q_INVOKABLE void startWrite();
    Q_INVOKABLE void startVerify();
    Q_INVOKABLE void cancel();
    Q_INVOKABLE void clearResult();

    // "<device>-<timestamp>.img" in the directory the user picked last time, as a URL a QML FileDialog
    // can preselect.
    Q_INVOKABLE QUrl suggestImageFileUrl() const;
    Q_INVOKABLE QUrl getImageDirectoryUrl() const;

    Q_INVOKABLE void showImageInFileManager() const;

Q_SIGNALS:
    void imagingRequested(const UDI::ImagingRequest& request);
    void imageFileChanged();
    void optionsChanged();
    void stateChanged();
    void progressChanged();
    void resultChanged();

public Q_SLOTS:
    void onOperationStarted(const UDI::ImagingRequest& request);
    void onProgressUpdated(const UDI::ImagingProgress& progress);
    void onOperationFinished(const UDI::ImagingResult& result);

private:
    void startOperation(ImagingOperation operation);
    // Turns a refusal into the same result panel a failed run uses, so there is one place to look.
    void reportRefusal(ImagingOperation operation, const QString& message);

    QPointer<DeviceListModel> m_deviceListModel;

    QString m_imageFilePath;
    QString m_trimMode;
    bool m_verifyAfterWrite{ true };
    bool m_computeSha256{ false };

    bool m_busy{ false };
    bool m_cancelling{ false };
    ImagingOperation m_operation{ ImagingOperation::None };

    ImagingProgress m_progress;
    ImagingResult m_result;
    bool m_hasResult{ false };
};
} // namespace UDI
