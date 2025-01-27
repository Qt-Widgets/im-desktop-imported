#pragma once

#include "Masks.h"

Q_DECLARE_LOGGING_CATEGORY(maskManager)

namespace Ui
{
    class core_dispatcher;
}

namespace Data
{
    struct Mask;
}

namespace voip_masks
{
    class MaskManager : public QObject
    {
        Q_OBJECT
    public:
        explicit MaskManager(Ui::core_dispatcher& _dispatcher, QObject* _parent);

        const MaskList& getAvailableMasks() const;

        void loadMask(const QString& _id);


    private slots:
        void onImCreated();
        void onMaskListLoaded(const QVector<QString>& maskList);
        void onPreviewLoaded(qint64 _seq, const QString& _localPath);
        void onModelLoaded();
        void onLoaded(qint64 _seq, const QString& _localPath);
        void onUpdateTimeout();
        void onRetryUpdate();
        void onExistentMasksLoaded(const std::vector<Data::Mask> &_masks);

    private:
        void loadMaskList();
        void loadPreviews();
        void loadModel();
        void loadExistentMasks();

        void loadNextPreview();

        qint64 postMessageToCore(std::string_view _message, const QString& _maskId) const;

    private:
        Ui::core_dispatcher& dispatcher_;
        MaskList masks_;

        QHash<QString, QPointer<Mask>> masksIndex_;

        MaskList currentDownloadPreviewList;

        QHash<qint64, QPointer<Mask>> previewSeqList_;
        QHash<qint64, QPointer<Mask>> maskSeqList_;

        QTimer* updateTimer_;

        bool existentListLoaded_ = false;
    };
}
