#include "stdafx.h"
#include "AvatarStorage.h"

#include "../../core_dispatcher.h"
#include "../../main_window/contact_list/ContactListModel.h"
#include "../../main_window/friendly/FriendlyContainer.h"
#include "../../main_window/history_control/HistoryControlPage.h"
#include "../../utils/gui_coll_helper.h"
#include "../../utils/utils.h"
#include "../../utils/InterConnector.h"

namespace
{
    QString CreateKey(const QString& _aimId, const int _sizePx);

    constexpr std::chrono::milliseconds CLEANUP_TIMEOUT = std::chrono::minutes(5);
    constexpr int64_t INITIAL_REQUEST_AVATAR_SEQ = -1;
}

namespace Logic
{
    AvatarStorage::AvatarStorage()
        : Timer_(new QTimer(this))
    {
        Timer_->setSingleShot(false);
        Timer_->setInterval(CLEANUP_TIMEOUT.count());
        Timer_->start();

        connect(Ui::GetDispatcher(), &Ui::core_dispatcher::avatarLoaded, this, &AvatarStorage::avatarLoaded);
        connect(Ui::GetDispatcher(), &Ui::core_dispatcher::avatarUpdated, this, [this](const QString& contact) {
            updateAvatar(contact);
        });
        connect(Timer_, &QTimer::timeout, this, &AvatarStorage::cleanup);
    }

    AvatarStorage::~AvatarStorage()
    {
    }

    const QPixmapSCptr& AvatarStorage::Get(const QString& _aimId, const QString& _displayName, const int _sizePx, bool& _isDefault, bool _regenerate)
    {
        assert(_sizePx > 0);

        TimesCache_[_aimId] = QDateTime::currentDateTime();

        Out _isDefault = !LoadedAvatars_.contains(_aimId);
        const auto key = CreateKey(_aimId, _sizePx);

        auto iterByAimIdAndSize = AvatarsByAimIdAndSize_.find(key);
        if (iterByAimIdAndSize != AvatarsByAimIdAndSize_.end())
        {
            return iterByAimIdAndSize->second;
        }

        auto iterByAimId = AvatarsByAimId_.find(_aimId);
        if (iterByAimId == AvatarsByAimId_.end())
        {
            auto drawDisplayName = _displayName.trimmed();
            if (drawDisplayName.isEmpty() && !_aimId.isEmpty())
            {
                drawDisplayName = Logic::GetFriendlyContainer()->getFriendly(_aimId);
            }

            auto defaultAvatar = Utils::getDefaultAvatar(_aimId, drawDisplayName, _sizePx);
            assert(!defaultAvatar.isNull());

            const auto result = AvatarsByAimId_.emplace(_aimId, std::make_shared<QPixmap>(std::move(defaultAvatar)));
            assert(result.second);

            iterByAimId = result.first;
        }

        const auto &avatarByAimId = *iterByAimId->second;
        assert(!avatarByAimId.isNull());

        const auto regenerateAvatar = ((avatarByAimId.width() < _sizePx) && _isDefault) && _aimId != _displayName;
        if (regenerateAvatar || _regenerate)
        {
            AvatarsByAimId_.erase(iterByAimId);
            CleanupSecondaryCaches(_aimId);
            return Get(_aimId, _displayName, _sizePx, _isDefault, _regenerate);
        }

        int avatarWidth = avatarByAimId.width();
        int avatarHeight = avatarByAimId.height();
        QPixmap scaledImage;
        if (avatarHeight >= avatarWidth)
            scaledImage = avatarByAimId.scaledToWidth(_sizePx, Qt::SmoothTransformation);
        else
            scaledImage = avatarByAimId.scaledToHeight(_sizePx, Qt::SmoothTransformation);

        const auto result = AvatarsByAimIdAndSize_.emplace(key, std::make_shared<QPixmap>(std::move(scaledImage)));
        assert(result.second);

        if (_aimId == ql1s("mail"))
            return result.first->second;

        auto requestedAvatarsIter = RequestedAvatars_.find(_aimId);
        if (requestedAvatarsIter == RequestedAvatars_.end() || (avatarByAimId.width() < _sizePx && avatarByAimId.height() < _sizePx))
        {
            Ui::gui_coll_helper collection(Ui::GetDispatcher()->create_collection(), true);
            collection.set_value_as_qstring("contact", _aimId);
            collection.set_value_as_int("size", _sizePx); //request only needed size

            auto seq = Ui::GetDispatcher()->post_message_to_core("avatars/get", collection.get());

            requests_.insert(seq);
            RequestedAvatars_.insert(_aimId);
        }
        else
        {
            Ui::gui_coll_helper collection(Ui::GetDispatcher()->create_collection(), true);
            collection.set_value_as_qstring("contact", _aimId);
            collection.set_value_as_int("size", _sizePx); //request only needed size

            Ui::GetDispatcher()->post_message_to_core("avatars/show", collection.get());
        }

        return result.first->second;
    }

    void AvatarStorage::SetAvatar(const QString& _aimId, const QPixmap& _pixmap)
    {
        assert(!_aimId.isEmpty());

        auto iter = AvatarsByAimId_.find(_aimId);
        if (iter == AvatarsByAimId_.end())
            return;

        const auto size = iter->second->height();
        auto scaled = _pixmap.scaled(size, size, Qt::KeepAspectRatio, Qt::SmoothTransformation);

        AvatarsByAimId_[_aimId] = std::make_shared<QPixmap>(std::move(scaled));

        CleanupSecondaryCaches(_aimId);

        LoadedAvatars_.removeAll(_aimId);
        LoadedAvatars_ << _aimId;

        emit avatarChanged(_aimId);
    }

    const AvatarStorage::CacheMap &AvatarStorage::GetByAimIdAndSize() const
    {
        return AvatarsByAimIdAndSize_;
    }

    const AvatarStorage::CacheMap &AvatarStorage::GetByAimId() const
    {
        return AvatarsByAimId_;
    }

    const AvatarStorage::CacheMap &AvatarStorage::GetRoundedByAimId() const
    {
        return AvatarsByAimId_;
    }

    void AvatarStorage::updateAvatar(const QString& _aimId, bool force)
    {
        if (!force && LoadedAvatars_.contains(_aimId))
            return;

        if (TimesCache_.find(_aimId) != TimesCache_.end())
        {
            auto iter = AvatarsByAimId_.find(_aimId);
            if (iter != AvatarsByAimId_.end())
            {
                Ui::gui_coll_helper collection(Ui::GetDispatcher()->create_collection(), true);
                collection.set_value_as_qstring("contact", _aimId);
                collection.set_value_as_int("size", iter->second->height());
                collection.set_value_as_bool("force", true);
                auto seq = Ui::GetDispatcher()->post_message_to_core("avatars/get", collection.get());
                requests_.insert(seq);
            }

            AvatarsByAimId_.erase(_aimId);
            CleanupSecondaryCaches(_aimId, false);
            RequestedAvatars_.erase(_aimId);
            LoadedAvatars_.removeAll(_aimId);
            TimesCache_.erase(_aimId);
        }
    }

    void AvatarStorage::ForceRequest(const QString& _aimId, const int _sizePx)
    {
        Ui::gui_coll_helper collection(Ui::GetDispatcher()->create_collection(), true);
        collection.set_value_as_qstring("contact", _aimId);
        collection.set_value_as_int("size", _sizePx);
        collection.set_value_as_bool("force", true);
        auto seq = Ui::GetDispatcher()->post_message_to_core("avatars/get", collection.get());
        requests_.insert(seq);
    }

    const QPixmapSCptr& AvatarStorage::GetRounded(const QString& _aimId, const QString& _displayName, const int _sizePx, const QString& _state, bool& _isDefault, bool _regenerate, bool mini_icons)
    {
        assert(_sizePx > 0);

        const auto &avatar = Get(_aimId, _displayName, _sizePx, _isDefault, _regenerate);
        if (avatar->isNull())
        {
            assert(!"avatar is null");
            return avatar;
        }

        return GetRounded(*avatar, _aimId, QString(), mini_icons, _isDefault);
    }

    QString AvatarStorage::GetLocal(const QString& _aimId, const QString& _displayName, const int _sizePx)
    {
        bool isDefault = false;
        QPixmapSCptr avatar = Get(_aimId, _displayName, _sizePx, isDefault, false);

        QFile file(QDir::tempPath() % qsl("/%1_%2.png").arg(_aimId, isDefault ? qsl("_def") : qsl("_")));
        if (!file.exists())
        {
            file.open(QIODevice::WriteOnly);
            avatar->save(&file, "png");
        }
        return QFileInfo(file).absoluteFilePath();
    }

    void AvatarStorage::avatarLoaded(int64_t _seq, const QString& _aimId, QPixmap& _pixmap, int _size, bool _result)
    {
        if (_seq != INITIAL_REQUEST_AVATAR_SEQ && !requests_.count(_seq))
            return;

        requests_.erase(_seq);

        if (!_result)
            return;

        if (_seq == INITIAL_REQUEST_AVATAR_SEQ)
        {
            RequestedAvatars_.insert(_aimId);
            TimesCache_[_aimId] = QDateTime::currentDateTime();
        }

        if (_pixmap.isNull())
        {
            if (!LoadedAvatarsFails_.contains(_aimId))
            {
                LoadedAvatarsFails_ << _aimId;
                Ui::gui_coll_helper collection(Ui::GetDispatcher()->create_collection(), true);
                collection.set_value_as_qstring("contact", _aimId);
                collection.set_value_as_int("size", _size);
                collection.set_value_as_bool("force", true);

                auto seq = Ui::GetDispatcher()->post_message_to_core("avatars/get", collection.get());
                requests_.insert(seq);
            }

            return;
        }

        assert(!_aimId.isEmpty());
        LoadedAvatarsFails_.removeAll(_aimId);
        auto scaledImage = _pixmap.scaled(_size, _size, Qt::KeepAspectRatio, Qt::SmoothTransformation);
        AvatarsByAimId_[_aimId] = std::make_shared<QPixmap>(std::move(scaledImage));
        CleanupSecondaryCaches(_aimId);

        LoadedAvatars_ << _aimId;

        emit avatarChanged(_aimId);
    }

    void AvatarStorage::UpdateDefaultAvatarIfNeed(const QString& _aimId)
    {
        assert(!_aimId.isEmpty());

        if (LoadedAvatars_.contains(_aimId))
            return;

        if (AvatarsByAimId_.find(_aimId) != AvatarsByAimId_.end())
            AvatarsByAimId_.erase(_aimId);
        CleanupSecondaryCaches(_aimId);
        emit avatarChanged(_aimId);
    }

    void AvatarStorage::cleanup()
    {
        const QDateTime now = QDateTime::currentDateTime();
        auto historyPage = Utils::InterConnector::instance().getHistoryPage(Logic::getContactListModel()->selectedContact());
        for (auto iter = TimesCache_.begin(); iter != TimesCache_.end();)
        {
            if (iter->second.msecsTo(now) >= CLEANUP_TIMEOUT.count())
            {
                auto aimId = iter->first;
                if (Logic::getContactListModel()->contains(aimId) || (historyPage && historyPage->contains(aimId)))
                {
                    iter->second = now;
                    ++iter;
                    continue;
                }

                AvatarsByAimId_.erase(aimId);
                CleanupSecondaryCaches(aimId);
                RequestedAvatars_.erase(aimId);
                LoadedAvatars_.removeAll(aimId);
                iter = TimesCache_.erase(iter);
            }
            else
            {
                ++iter;
            }
        }
    }

    // TODO : use two-step hash here
    void AvatarStorage::CleanupSecondaryCaches(const QString& _aimId, bool _isRoundedAvatarsClean)
    {
        const auto cleanupSecondaryCache = [&_aimId](CacheMap &cache)
        {
            for (auto i = cache.begin(); i != cache.end(); ++i)
            {
                const auto &key = i->first;
                if (!key.startsWith(_aimId))
                {
                    continue;
                }

                for(;;)
                {
                    i = cache.erase(i);

                    if (i == cache.end())
                    {
                        break;
                    }

                    const auto &k = i->first;
                    if (!k.startsWith(_aimId))
                    {
                        break;
                    }
                }

                break;
            }
        };

        if (_isRoundedAvatarsClean)
            cleanupSecondaryCache(RoundedAvatarsByAimIdAndSize_);

        cleanupSecondaryCache(AvatarsByAimIdAndSize_);
    }

    const QPixmapSCptr& AvatarStorage::GetRounded(const QPixmap& _avatar, const QString& _aimId, const QString& _state, bool mini_icons, bool _isDefault)
    {
        assert(!_avatar.isNull());

        const QString key = _aimId
        % ql1c('/')
        % QString::number(_avatar.width())
        % ql1c('/')
        % _state;

        auto i = RoundedAvatarsByAimIdAndSize_.find(key);
        if (i == RoundedAvatarsByAimIdAndSize_.end())
        {
            auto roundedAvatar = Utils::roundImage(_avatar, _state, _isDefault, mini_icons);
            i = RoundedAvatarsByAimIdAndSize_
                .emplace(
                    key,
                    std::make_shared<QPixmap>(std::move(roundedAvatar)))
                .first;
        }

        return i->second;
    }

    AvatarStorage* GetAvatarStorage()
    {
        static std::unique_ptr<AvatarStorage> storage(new AvatarStorage());
        return storage.get();
    }
}

namespace
{
    QString CreateKey(const QString &_aimId, const int _sizePx)
    {
        assert(_sizePx > 0);
        return _aimId % ql1c('/') % QString::number(_sizePx);
    }
}
