/*
 * Copyright 2017-2019 Amazon.com, Inc. or its affiliates. All Rights Reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License").
 * You may not use this file except in compliance with the License.
 * A copy of the License is located at
 *
 *     http://aws.amazon.com/apache2.0/
 *
 * or in the "license" file accompanying this file. This file is distributed
 * on an "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either
 * express or implied. See the License for the specific language governing
 * permissions and limitations under the License.
 */

/// @file ExternalMediaPlayer.cpp
#include "ExternalMediaPlayer/ExternalMediaPlayer.h"

#include <rapidjson/stringbuffer.h>
#include <rapidjson/writer.h>
#include <rapidjson/error/en.h>

#include <AVSCommon/AVS/ExternalMediaPlayer/AdapterUtils.h>
#include <AVSCommon/AVS/ExternalMediaPlayer/ExternalMediaAdapterConstants.h>
#include <AVSCommon/AVS/SpeakerConstants/SpeakerConstants.h>
#include <AVSCommon/Utils/JSON/JSONUtils.h>
#include <AVSCommon/Utils/Memory/Memory.h>

namespace alexaClientSDK {
namespace capabilityAgents {
namespace externalMediaPlayer {

using namespace avsCommon::avs;
using namespace avsCommon::avs::externalMediaPlayer;
using namespace avsCommon::sdkInterfaces;
using namespace avsCommon::sdkInterfaces::externalMediaPlayer;
using namespace avsCommon::avs::attachment;
using namespace avsCommon::utils;
using namespace avsCommon::sdkInterfaces;
using namespace avsCommon::utils::json;
using namespace avsCommon::utils::logger;

/// String to identify log entries originating from this file.
static const std::string TAG("ExternalMediaPlayer");

/**
 * Create a LogEntry using this file's TAG and the specified event string.
 *
 * @param The event string for this @c LogEntry.
 */
#define LX(event) alexaClientSDK::avsCommon::utils::logger::LogEntry(TAG, event)

// The namespaces used in the context.
static const std::string EXTERNALMEDIAPLAYER_STATE_NAMESPACE = "ExternalMediaPlayer";
static const std::string PLAYBACKSTATEREPORTER_STATE_NAMESPACE = "Alexa.PlaybackStateReporter";

// The names used in the context.
static const std::string EXTERNALMEDIAPLAYER_NAME = "ExternalMediaPlayerState";
static const std::string PLAYBACKSTATEREPORTER_NAME = "playbackState";

// The namespace for this capability agent.
static const std::string EXTERNALMEDIAPLAYER_NAMESPACE = "ExternalMediaPlayer";
static const std::string PLAYBACKCONTROLLER_NAMESPACE = "Alexa.PlaybackController";
static const std::string PLAYLISTCONTROLLER_NAMESPACE = "Alexa.PlaylistController";
static const std::string SEEKCONTROLLER_NAMESPACE = "Alexa.SeekController";
static const std::string FAVORITESCONTROLLER_NAMESPACE = "Alexa.FavoritesController";

// Capability constants
/// The AlexaInterface constant type.
static const std::string ALEXA_INTERFACE_TYPE = "AlexaInterface";

/// ExternalMediaPlayer capability constants
/// ExternalMediaPlayer interface type
static const std::string EXTERNALMEDIAPLAYER_CAPABILITY_INTERFACE_TYPE = ALEXA_INTERFACE_TYPE;
/// ExternalMediaPlayer interface name
static const std::string EXTERNALMEDIAPLAYER_CAPABILITY_INTERFACE_NAME = "ExternalMediaPlayer";
/// ExternalMediaPlayer interface version
#ifdef EXTERNALMEDIAPLAYER_1_1
static const std::string EXTERNALMEDIAPLAYER_CAPABILITY_INTERFACE_VERSION = "1.1";
#else
static const std::string EXTERNALMEDIAPLAYER_CAPABILITY_INTERFACE_VERSION = "1.0";
#endif

#ifdef EXTERNALMEDIAPLAYER_1_1
/// Alexa.PlaybackStateReporter name.
static const std::string PLAYBACKSTATEREPORTER_CAPABILITY_INTERFACE_NAME = PLAYBACKSTATEREPORTER_STATE_NAMESPACE;
/// Alexa.PlaybackStateReporter version.
static const std::string PLAYBACKSTATEREPORTER_CAPABILITY_INTERFACE_VERSION = "1.0";

/// Alexa.PlaybackController name.
static const std::string PLAYBACKCONTROLLER_CAPABILITY_INTERFACE_NAME = PLAYBACKCONTROLLER_NAMESPACE;
/// Alexa.PlaybackController version.
static const std::string PLAYBACKCONTROLLER_CAPABILITY_INTERFACE_VERSION = "1.0";

/// Alexa.PlaylistController name.
static const std::string PLAYLISTCONTROLLER_CAPABILITY_INTERFACE_NAME = PLAYLISTCONTROLLER_NAMESPACE;
/// Alexa.PlaylistController version.
static const std::string PLAYLISTCONTROLLER_CAPABILITY_INTERFACE_VERSION = "1.0";

/// Alexa.SeekController name.
static const std::string SEEKCONTROLLER_CAPABILITY_INTERFACE_NAME = SEEKCONTROLLER_NAMESPACE;
/// Alexa.SeekController version.
static const std::string SEEKCONTROLLER_CAPABILITY_INTERFACE_VERSION = "1.0";

/// Alexa.FavoritesController name.
static const std::string FAVORITESCONTROLLER_CAPABILITY_INTERFACE_NAME = FAVORITESCONTROLLER_NAMESPACE;
/// Alexa.FavoritesController version.
static const std::string FAVORITESCONTROLLER_CAPABILITY_INTERFACE_VERSION = "1.0";
#endif

#ifdef EXTERNALMEDIAPLAYER_1_1
/// The name of the @c FocusManager channel used by @c ExternalMediaPlayer and
/// its Adapters.
static const std::string CHANNEL_NAME = avsCommon::sdkInterfaces::FocusManagerInterface::CONTENT_CHANNEL_NAME;

/**
 * The activityId string used with @c FocusManager by @c ExternalMediaPlayer.
 * (as per spec for AVS for monitoring channel activity.)
 */
static const std::string FOCUS_MANAGER_ACTIVITY_ID = "ExternalMediaPlayer";

/// The duration to wait for a state change in @c onFocusChanged before failing.
static const std::chrono::seconds TIMEOUT{2};
#endif

// The @c External media player play directive signature.
static const NamespaceAndName PLAY_DIRECTIVE{EXTERNALMEDIAPLAYER_NAMESPACE, "Play"};
static const NamespaceAndName LOGIN_DIRECTIVE{EXTERNALMEDIAPLAYER_NAMESPACE, "Login"};
static const NamespaceAndName LOGOUT_DIRECTIVE{EXTERNALMEDIAPLAYER_NAMESPACE, "Logout"};
#ifdef EXTERNALMEDIAPLAYER_1_1
static const NamespaceAndName AUTHORIZEDISCOVEREDPLAYERS_DIRECTIVE{EXTERNALMEDIAPLAYER_NAMESPACE, "AuthorizeDiscoveredPlayers"};
#endif

// The @c Transport control directive signatures.
static const NamespaceAndName RESUME_DIRECTIVE{PLAYBACKCONTROLLER_NAMESPACE, "Play"};
static const NamespaceAndName PAUSE_DIRECTIVE{PLAYBACKCONTROLLER_NAMESPACE, "Pause"};
static const NamespaceAndName STOP_DIRECTIVE{PLAYBACKCONTROLLER_NAMESPACE, "Stop"};
static const NamespaceAndName NEXT_DIRECTIVE{PLAYBACKCONTROLLER_NAMESPACE, "Next"};
static const NamespaceAndName PREVIOUS_DIRECTIVE{PLAYBACKCONTROLLER_NAMESPACE, "Previous"};
static const NamespaceAndName STARTOVER_DIRECTIVE{PLAYBACKCONTROLLER_NAMESPACE, "StartOver"};
static const NamespaceAndName REWIND_DIRECTIVE{PLAYBACKCONTROLLER_NAMESPACE, "Rewind"};
static const NamespaceAndName FASTFORWARD_DIRECTIVE{PLAYBACKCONTROLLER_NAMESPACE, "FastForward"};

// The @c PlayList control directive signature.
static const NamespaceAndName ENABLEREPEATONE_DIRECTIVE{PLAYLISTCONTROLLER_NAMESPACE, "EnableRepeatOne"};
static const NamespaceAndName ENABLEREPEAT_DIRECTIVE{PLAYLISTCONTROLLER_NAMESPACE, "EnableRepeat"};
static const NamespaceAndName DISABLEREPEAT_DIRECTIVE{PLAYLISTCONTROLLER_NAMESPACE, "DisableRepeat"};
static const NamespaceAndName ENABLESHUFFLE_DIRECTIVE{PLAYLISTCONTROLLER_NAMESPACE, "EnableShuffle"};
static const NamespaceAndName DISABLESHUFFLE_DIRECTIVE{PLAYLISTCONTROLLER_NAMESPACE, "DisableShuffle"};

// The @c Seek control directive signature.
static const NamespaceAndName SEEK_DIRECTIVE{SEEKCONTROLLER_NAMESPACE, "SetSeekPosition"};
static const NamespaceAndName ADJUSTSEEK_DIRECTIVE{SEEKCONTROLLER_NAMESPACE, "AdjustSeekPosition"};

// The @c favorites control directive signature.
static const NamespaceAndName FAVORITE_DIRECTIVE{FAVORITESCONTROLLER_NAMESPACE, "Favorite"};
static const NamespaceAndName UNFAVORITE_DIRECTIVE{FAVORITESCONTROLLER_NAMESPACE, "Unfavorite"};

// The @c ExternalMediaPlayer context state signatures.
static const NamespaceAndName SESSION_STATE{EXTERNALMEDIAPLAYER_STATE_NAMESPACE, EXTERNALMEDIAPLAYER_NAME};
static const NamespaceAndName PLAYBACK_STATE{PLAYBACKSTATEREPORTER_STATE_NAMESPACE, PLAYBACKSTATEREPORTER_NAME};

/// The const char for the players key field in the context.
static const char PLAYERS[] = "players";

/// The const char for the playerInFocus key field in the context.
static const char PLAYER_IN_FOCUS[] = "playerInFocus";

/// The max relative time in the past that we can  seek to in milliseconds(-12hours in ms).
static const int64_t MAX_PAST_OFFSET = -86400000;

/// The max relative time in the past that we can  seek to in milliseconds(12 hours in ms).
static const int64_t MAX_FUTURE_OFFSET = 86400000;

/**
 * Creates the ExternalMediaPlayer capability configuration.
 *
 * @return The ExternalMediaPlayer capability configuration.
 */
static std::shared_ptr<avsCommon::avs::CapabilityConfiguration> getExternalMediaPlayerCapabilityConfiguration();

/// The @c m_directiveToHandlerMap Map of the directives to their handlers.
std::unordered_map<NamespaceAndName, std::pair<RequestType, ExternalMediaPlayer::DirectiveHandler>>
    ExternalMediaPlayer::m_directiveToHandlerMap = {
#ifdef EXTERNALMEDIAPLAYER_1_1
        {AUTHORIZEDISCOVEREDPLAYERS_DIRECTIVE, std::make_pair(RequestType::NONE, &ExternalMediaPlayer::handleAuthorizeDiscoveredPlayers)},
#endif
        {LOGIN_DIRECTIVE, std::make_pair(RequestType::LOGIN, &ExternalMediaPlayer::handleLogin)},
        {LOGOUT_DIRECTIVE, std::make_pair(RequestType::LOGOUT, &ExternalMediaPlayer::handleLogout)},
        {PLAY_DIRECTIVE, std::make_pair(RequestType::PLAY, &ExternalMediaPlayer::handlePlay)},
        {PAUSE_DIRECTIVE, std::make_pair(RequestType::PAUSE, &ExternalMediaPlayer::handlePlayControl)},
        {STOP_DIRECTIVE, std::make_pair(RequestType::STOP, &ExternalMediaPlayer::handlePlayControl)},
        {RESUME_DIRECTIVE, std::make_pair(RequestType::RESUME, &ExternalMediaPlayer::handlePlayControl)},
        {NEXT_DIRECTIVE, std::make_pair(RequestType::NEXT, &ExternalMediaPlayer::handlePlayControl)},
        {PREVIOUS_DIRECTIVE, std::make_pair(RequestType::PREVIOUS, &ExternalMediaPlayer::handlePlayControl)},
        {STARTOVER_DIRECTIVE, std::make_pair(RequestType::START_OVER, &ExternalMediaPlayer::handlePlayControl)},
        {FASTFORWARD_DIRECTIVE, std::make_pair(RequestType::FAST_FORWARD, &ExternalMediaPlayer::handlePlayControl)},
        {REWIND_DIRECTIVE, std::make_pair(RequestType::REWIND, &ExternalMediaPlayer::handlePlayControl)},
        {ENABLEREPEATONE_DIRECTIVE,
         std::make_pair(RequestType::ENABLE_REPEAT_ONE, &ExternalMediaPlayer::handlePlayControl)},
        {ENABLEREPEAT_DIRECTIVE, std::make_pair(RequestType::ENABLE_REPEAT, &ExternalMediaPlayer::handlePlayControl)},
        {DISABLEREPEAT_DIRECTIVE, std::make_pair(RequestType::DISABLE_REPEAT, &ExternalMediaPlayer::handlePlayControl)},
        {ENABLESHUFFLE_DIRECTIVE, std::make_pair(RequestType::ENABLE_SHUFFLE, &ExternalMediaPlayer::handlePlayControl)},
        {DISABLESHUFFLE_DIRECTIVE,
         std::make_pair(RequestType::DISABLE_SHUFFLE, &ExternalMediaPlayer::handlePlayControl)},
        {FAVORITE_DIRECTIVE, std::make_pair(RequestType::FAVORITE, &ExternalMediaPlayer::handlePlayControl)},
        {UNFAVORITE_DIRECTIVE, std::make_pair(RequestType::UNFAVORITE, &ExternalMediaPlayer::handlePlayControl)},
        {SEEK_DIRECTIVE, std::make_pair(RequestType::SEEK, &ExternalMediaPlayer::handleSeek)},
        {ADJUSTSEEK_DIRECTIVE, std::make_pair(RequestType::ADJUST_SEEK, &ExternalMediaPlayer::handleAdjustSeek)}};
// TODO: ARC-227 Verify default values
auto audioNonBlockingPolicy = BlockingPolicy(BlockingPolicy::MEDIUM_AUDIO, false);
auto neitherNonBlockingPolicy = BlockingPolicy(BlockingPolicy::MEDIUMS_NONE, false);

#ifdef EXTERNALMEDIAPLAYER_1_1
static DirectiveHandlerConfiguration g_configuration = {{AUTHORIZEDISCOVEREDPLAYERS_DIRECTIVE, audioNonBlockingPolicy},
                                                        {PLAY_DIRECTIVE, audioNonBlockingPolicy},
#else
static DirectiveHandlerConfiguration g_configuration = {{PLAY_DIRECTIVE, audioNonBlockingPolicy},
#endif
                                                        {LOGIN_DIRECTIVE, neitherNonBlockingPolicy},
                                                        {LOGOUT_DIRECTIVE, neitherNonBlockingPolicy},
                                                        {RESUME_DIRECTIVE, audioNonBlockingPolicy},
                                                        {PAUSE_DIRECTIVE, audioNonBlockingPolicy},
                                                        {STOP_DIRECTIVE, audioNonBlockingPolicy},
                                                        {NEXT_DIRECTIVE, audioNonBlockingPolicy},
                                                        {PREVIOUS_DIRECTIVE, audioNonBlockingPolicy},
                                                        {STARTOVER_DIRECTIVE, audioNonBlockingPolicy},
                                                        {REWIND_DIRECTIVE, audioNonBlockingPolicy},
                                                        {FASTFORWARD_DIRECTIVE, audioNonBlockingPolicy},
                                                        {ENABLEREPEATONE_DIRECTIVE, neitherNonBlockingPolicy},
                                                        {ENABLEREPEAT_DIRECTIVE, neitherNonBlockingPolicy},
                                                        {DISABLEREPEAT_DIRECTIVE, neitherNonBlockingPolicy},
                                                        {ENABLESHUFFLE_DIRECTIVE, neitherNonBlockingPolicy},
                                                        {DISABLESHUFFLE_DIRECTIVE, neitherNonBlockingPolicy},
                                                        {SEEK_DIRECTIVE, audioNonBlockingPolicy},
                                                        {ADJUSTSEEK_DIRECTIVE, audioNonBlockingPolicy},
                                                        {FAVORITE_DIRECTIVE, neitherNonBlockingPolicy},
                                                        {UNFAVORITE_DIRECTIVE, neitherNonBlockingPolicy}};

static std::unordered_map<PlaybackButton, RequestType> g_buttonToRequestType = {
#ifdef EXTERNALMEDIAPLAYER_1_1
    // Important Note: This changes default AVS Device SDK behavior.
    {PlaybackButton::PLAY, RequestType::RESUME},
    {PlaybackButton::PAUSE, RequestType::PAUSE},
#else
    {PlaybackButton::PLAY, RequestType::PAUSE_RESUME_TOGGLE},
    {PlaybackButton::PAUSE, RequestType::PAUSE_RESUME_TOGGLE},
#endif
    {PlaybackButton::NEXT, RequestType::NEXT},
    {PlaybackButton::PREVIOUS, RequestType::PREVIOUS}};

static std::unordered_map<PlaybackToggle, std::pair<RequestType, RequestType>> g_toggleToRequestType = {
    {PlaybackToggle::SHUFFLE, std::make_pair(RequestType::ENABLE_SHUFFLE, RequestType::DISABLE_SHUFFLE)},
    {PlaybackToggle::LOOP, std::make_pair(RequestType::ENABLE_REPEAT, RequestType::DISABLE_REPEAT)},
    {PlaybackToggle::REPEAT, std::make_pair(RequestType::ENABLE_REPEAT_ONE, RequestType::DISABLE_REPEAT)},
    {PlaybackToggle::THUMBS_UP, std::make_pair(RequestType::FAVORITE, RequestType::DESELECT_FAVORITE)},
    {PlaybackToggle::THUMBS_DOWN, std::make_pair(RequestType::UNFAVORITE, RequestType::DESELECT_UNFAVORITE)}};

/**
 * Generate a @c CapabilityConfiguration object.
 *
 * @param type The Capability interface type.
 * @param interface The Capability interface name.
 * @param version The Capability interface verison.
 */
static std::shared_ptr<CapabilityConfiguration> generateCapabilityConfiguration(
    const std::string& type,
    const std::string& interface,
    const std::string& version) {
    std::unordered_map<std::string, std::string> configMap;
    configMap.insert({CAPABILITY_INTERFACE_TYPE_KEY, type});
    configMap.insert({CAPABILITY_INTERFACE_NAME_KEY, interface});
    configMap.insert({CAPABILITY_INTERFACE_VERSION_KEY, version});

    return std::make_shared<CapabilityConfiguration>(configMap);
}

std::shared_ptr<ExternalMediaPlayer> ExternalMediaPlayer::create(
    const AdapterMediaPlayerMap& mediaPlayers,
    const AdapterSpeakerMap& speakers,
    const AdapterCreationMap& adapterCreationMap,
    std::shared_ptr<SpeakerManagerInterface> speakerManager,
    std::shared_ptr<MessageSenderInterface> messageSender,
    std::shared_ptr<FocusManagerInterface> focusManager,
    std::shared_ptr<ContextManagerInterface> contextManager,
    std::shared_ptr<ExceptionEncounteredSenderInterface> exceptionSender,
    std::shared_ptr<PlaybackRouterInterface> playbackRouter) {
    if (nullptr == speakerManager) {
        ACSDK_ERROR(LX("createFailed").d("reason", "nullSpeakerManager"));
        return nullptr;
    }

    if (nullptr == messageSender) {
        ACSDK_ERROR(LX("createFailed").d("reason", "nullMessageSender"));
        return nullptr;
    }
    if (nullptr == focusManager) {
        ACSDK_ERROR(LX("createFailed").d("reason", "nullFocusManager"));
        return nullptr;
    }
    if (nullptr == contextManager) {
        ACSDK_ERROR(LX("createFailed").d("reason", "nullContextManager"));
        return nullptr;
    }
    if (nullptr == exceptionSender) {
        ACSDK_ERROR(LX("createFailed").d("reason", "nullExceptionSender"));
        return nullptr;
    }
    if (nullptr == playbackRouter) {
        ACSDK_ERROR(LX("createFailed").d("reason", "nullPlaybackRouter"));
        return nullptr;
    }

    auto externalMediaPlayer = std::shared_ptr<ExternalMediaPlayer>(
        new ExternalMediaPlayer(speakerManager, contextManager, exceptionSender, playbackRouter));

    contextManager->setStateProvider(SESSION_STATE, externalMediaPlayer);
    contextManager->setStateProvider(PLAYBACK_STATE, externalMediaPlayer);

    externalMediaPlayer->createAdapters(
        mediaPlayers, speakers, adapterCreationMap, messageSender, focusManager, contextManager);

#ifdef EXTERNALMEDIAPLAYER_1_1
    externalMediaPlayer->m_focusManager = focusManager;
#endif

    return externalMediaPlayer;
}

ExternalMediaPlayer::ExternalMediaPlayer(
    std::shared_ptr<SpeakerManagerInterface> speakerManager,
    std::shared_ptr<ContextManagerInterface> contextManager,
    std::shared_ptr<ExceptionEncounteredSenderInterface> exceptionSender,
    std::shared_ptr<PlaybackRouterInterface> playbackRouter) :
        CapabilityAgent{EXTERNALMEDIAPLAYER_NAMESPACE, exceptionSender},
        RequiresShutdown{"ExternalMediaPlayer"},
        m_speakerManager{speakerManager},
        m_contextManager{contextManager},
#ifdef EXTERNALMEDIAPLAYER_1_1
        m_playbackRouter{playbackRouter},
        m_focus{FocusState::NONE},
        m_focusAcquireInProgress{false},
        m_haltInitiator{HaltInitiator::NONE},
        m_currentActivity{avsCommon::avs::PlayerActivity::IDLE} {
#else
        m_playbackRouter{playbackRouter} {
#endif
    m_capabilityConfigurations.insert(getExternalMediaPlayerCapabilityConfiguration());
#ifdef EXTERNALMEDIAPLAYER_1_1
    // Register all supported capabilities.
    m_capabilityConfigurations.insert(generateCapabilityConfiguration(
        ALEXA_INTERFACE_TYPE,
        PLAYBACKSTATEREPORTER_CAPABILITY_INTERFACE_NAME,
        PLAYBACKSTATEREPORTER_CAPABILITY_INTERFACE_VERSION));

    m_capabilityConfigurations.insert(generateCapabilityConfiguration(
        ALEXA_INTERFACE_TYPE,
        PLAYBACKCONTROLLER_CAPABILITY_INTERFACE_NAME,
        PLAYBACKCONTROLLER_CAPABILITY_INTERFACE_VERSION));

    m_capabilityConfigurations.insert(generateCapabilityConfiguration(
        ALEXA_INTERFACE_TYPE,
        PLAYLISTCONTROLLER_CAPABILITY_INTERFACE_NAME,
        PLAYLISTCONTROLLER_CAPABILITY_INTERFACE_VERSION));

    m_capabilityConfigurations.insert(generateCapabilityConfiguration(
        ALEXA_INTERFACE_TYPE, SEEKCONTROLLER_CAPABILITY_INTERFACE_NAME, SEEKCONTROLLER_CAPABILITY_INTERFACE_VERSION));

    m_capabilityConfigurations.insert(generateCapabilityConfiguration(
        ALEXA_INTERFACE_TYPE,
        FAVORITESCONTROLLER_CAPABILITY_INTERFACE_NAME,
        FAVORITESCONTROLLER_CAPABILITY_INTERFACE_VERSION));
#endif
}

std::shared_ptr<CapabilityConfiguration> getExternalMediaPlayerCapabilityConfiguration() {
    return generateCapabilityConfiguration(
        EXTERNALMEDIAPLAYER_CAPABILITY_INTERFACE_TYPE,
        EXTERNALMEDIAPLAYER_CAPABILITY_INTERFACE_NAME,
        EXTERNALMEDIAPLAYER_CAPABILITY_INTERFACE_VERSION);
}

#ifdef EXTERNALMEDIAPLAYER_1_1
void ExternalMediaPlayer::addAdapterHandler(
    std::shared_ptr<avsCommon::sdkInterfaces::ExternalMediaAdapterHandlerInterface> adapterHandler) {
    ACSDK_DEBUG5(LX("addAdapterHandler"));
    if (!adapterHandler) {
        ACSDK_ERROR(LX("addAdapterHandler").m("Adapter handler is null."));
        return;
    }
    m_executor.submit([this, adapterHandler]() {
        ACSDK_DEBUG5(LX("addAdapterHandlerInExecutor"));
        if (!m_adapterHandlers.insert(adapterHandler).second) {
            ACSDK_ERROR(LX("addAdapterHandlerInExecutor").m("Duplicate adapter handler."));
        }
    });
}

void ExternalMediaPlayer::removeAdapterHandler(
    std::shared_ptr<avsCommon::sdkInterfaces::ExternalMediaAdapterHandlerInterface> adapterHandler) {
    ACSDK_DEBUG5(LX("removeAdapterHandler"));
    if (!adapterHandler) {
        ACSDK_ERROR(LX("removeAdapterHandler").m("Adapter handler is null."));
        return;
    }
    m_executor.submit([this, adapterHandler]() {
        ACSDK_DEBUG5(LX("removeAdapterHandlerInExecutor"));
        if (m_adapterHandlers.erase(adapterHandler) == 0) {
            ACSDK_WARN(LX("removeAdapterHandlerInExecutor").m("Nonexistent adapter handler."));
        }
    });
}
#endif

#ifdef EXTERNALMEDIAPLAYER_1_1
void ExternalMediaPlayer::executeOnFocusChanged(avsCommon::avs::FocusState newFocus) {
    ACSDK_DEBUG1(
        LX("executeOnFocusChanged").d("from", m_focus).d("to", newFocus).d("m_currentActivity", m_currentActivity));
    if (m_focus == newFocus) {
        m_focusAcquireInProgress = false;
        return;
    }
    m_focus = newFocus;
    m_focusAcquireInProgress = false;

    if (!m_playerInFocus.empty()) {
        auto adapterIt = m_adapters.find(m_playerInFocus);

        if (m_adapters.end() == adapterIt) {
            switch (newFocus) {
                case FocusState::FOREGROUND: {
                    /*
                     * If the system is currently in a pause initiated from AVS, on focus change
                     * to FOREGROUND do not try to resume. This happens when a user calls
                     * "Alexa, pause" while Spotify is PLAYING. This moves the adapter to
                     * BACKGROUND focus. AVS then sends a PAUSE request and after calling the
                     * ESDK pause when the adapter switches to FOREGROUND focus we do not want
                     * the adapter to start PLAYING.
                     */
                    if (m_haltInitiator == HaltInitiator::EXTERNAL_PAUSE) {
                        return;
                    }

                    switch (m_currentActivity) {
                        case PlayerActivity::IDLE:
                        case PlayerActivity::STOPPED:
                        case PlayerActivity::FINISHED:
                            return;
                        case PlayerActivity::PAUSED: {
                            // A focus change to foreground when paused means we should resume the current song.
                            ACSDK_DEBUG1(LX("executeOnFocusChanged").d("action", "resumeExternalMediaPlayer"));
                            setCurrentActivity(avsCommon::avs::PlayerActivity::PLAYING);
                            // At this point a request to play another artist on Spotify may have already
                            // been processed (or is being processed) and we do not want to send resume here.
                            if (m_haltInitiator != HaltInitiator::NONE) {
                                for (auto adapterHandler : m_adapterHandlers) {
                                    adapterHandler->playControlForPlayer(m_playerInFocus, RequestType::RESUME);
                                }
                            }
                        }
                            return;
                        case PlayerActivity::PLAYING:
                        case PlayerActivity::BUFFER_UNDERRUN:
                            // We should already have foreground focus in these states; break out to the warning below.
                            break;
                    }
                    break;
                }
                case FocusState::BACKGROUND:
                    switch (m_currentActivity) {
                        case PlayerActivity::STOPPED:
                        // We can also end up here with an empty queue if we've asked MediaPlayer to play, but playback
                        // hasn't started yet, so we fall through to call @c pause() here as well.
                        case PlayerActivity::FINISHED:
                        case PlayerActivity::IDLE:
                        // Note: can be in FINISHED or IDLE while waiting for MediaPlayer to start playing, so we fall
                        // through to call @c pause() here as well.
                        case PlayerActivity::PAUSED:
                        // Note: can be in PAUSED while we're trying to resume, in which case we still want to pause, so we
                        // fall through to call @c pause() here as well.
                        case PlayerActivity::PLAYING:
                        case PlayerActivity::BUFFER_UNDERRUN: {
                            // If we get pushed into the background while playing or buffering, pause the current song.
                            ACSDK_DEBUG1(LX("executeOnFocusChanged").d("action", "pauseExternalMediaPlayer"));
                            if (m_haltInitiator != HaltInitiator::EXTERNAL_PAUSE) {
                                m_haltInitiator = HaltInitiator::FOCUS_CHANGE_PAUSE;
                            }
                            setCurrentActivity(avsCommon::avs::PlayerActivity::PAUSED);
                            for (auto adapterHandler : m_adapterHandlers) {
                                adapterHandler->playControlForPlayer(m_playerInFocus, RequestType::PAUSE);
                            }
                        }
                            return;
                    }
                    break;
                case FocusState::NONE:
                    switch (m_currentActivity) {
                        case PlayerActivity::IDLE:
                        case PlayerActivity::STOPPED:
                        case PlayerActivity::FINISHED:
                            // Nothing to more to do if we're already not playing; we got here because the act of stopping
                            // caused the channel to be released, which in turn caused this callback.
                            return;
                        case PlayerActivity::PLAYING:
                        case PlayerActivity::PAUSED:
                        case PlayerActivity::BUFFER_UNDERRUN:
                            // If the focus change came in while we were in a 'playing' state, we need to stop because we are
                            // yielding the channel.
                            ACSDK_DEBUG1(LX("executeOnFocusChanged").d("action", "stopExternalMediaPlayer"));
                            m_haltInitiator = HaltInitiator::FOCUS_CHANGE_STOP;
                            setCurrentActivity(avsCommon::avs::PlayerActivity::STOPPED);
                            for (auto adapterHandler : m_adapterHandlers) {
                                adapterHandler->playControlForPlayer(m_playerInFocus, RequestType::STOP);
                            }
                            return;
                    }
                    break;
            }
        }
    }
    ACSDK_WARN(LX("unexpectedExecuteOnFocusChanged").d("newFocus", newFocus).d("m_currentActivity", m_currentActivity));
}

void ExternalMediaPlayer::onFocusChanged(FocusState newFocus) {
    ACSDK_DEBUG(LX("onFocusChanged").d("newFocus", newFocus));
    m_executor.submit([this, newFocus] { executeOnFocusChanged(newFocus); });

    switch (newFocus) {
        case FocusState::FOREGROUND:
            // Could wait for playback to actually start, but there's no real benefit to waiting, and long delays in
            // buffering could result in timeouts, so returning immediately for this case.
            return;
        case FocusState::BACKGROUND: {
            // Ideally expecting to see a transition to PAUSED, but in terms of user-observable changes, a move to any
            // of PAUSED/STOPPED/FINISHED will indicate that it's safe for another channel to move to the foreground.
            auto predicate = [this] {
                switch (m_currentActivity) {
                    case PlayerActivity::IDLE:
                    case PlayerActivity::PAUSED:
                    case PlayerActivity::STOPPED:
                    case PlayerActivity::FINISHED:
                        return true;
                    case PlayerActivity::PLAYING:
                    case PlayerActivity::BUFFER_UNDERRUN:
                        return false;
                }
                ACSDK_ERROR(LX("onFocusChangedFailed")
                                .d("reason", "unexpectedActivity")
                                .d("m_currentActivity", m_currentActivity));
                return false;
            };
            std::unique_lock<std::mutex> lock(m_currentActivityMutex);
            if (!m_currentActivityConditionVariable.wait_for(lock, TIMEOUT, predicate)) {
                ACSDK_ERROR(
                    LX("onFocusChangedTimedOut").d("newFocus", newFocus).d("m_currentActivity", m_currentActivity));
            }
        }
            return;
        case FocusState::NONE: {
            // Need to wait for STOPPED or FINISHED, indicating that we have completely ended playback.
            auto predicate = [this] {
                switch (m_currentActivity) {
                    case PlayerActivity::IDLE:
                    case PlayerActivity::STOPPED:
                    case PlayerActivity::FINISHED:
                        return true;
                    case PlayerActivity::PLAYING:
                    case PlayerActivity::PAUSED:
                    case PlayerActivity::BUFFER_UNDERRUN:
                        return false;
                }
                ACSDK_ERROR(LX("onFocusChangedFailed")
                                .d("reason", "unexpectedActivity")
                                .d("m_currentActivity", m_currentActivity));
                return false;
            };
            std::unique_lock<std::mutex> lock(m_currentActivityMutex);
            if (!m_currentActivityConditionVariable.wait_for(lock, TIMEOUT, predicate)) {
                ACSDK_ERROR(LX("onFocusChangedFailed")
                                .d("reason", "activityChangeTimedOut")
                                .d("newFocus", newFocus)
                                .d("m_currentActivity", m_currentActivity));
            }
        }
            return;
    }
    ACSDK_ERROR(LX("onFocusChangedFailed").d("reason", "unexpectedFocusState").d("newFocus", newFocus));
}

void ExternalMediaPlayer::onContextAvailable(const std::string&) {
    // default no-op
}

void ExternalMediaPlayer::onContextFailure(const avsCommon::sdkInterfaces::ContextRequestError) {
    // default no-op
}
#endif

void ExternalMediaPlayer::provideState(
    const avsCommon::avs::NamespaceAndName& stateProviderName,
    unsigned int stateRequestToken) {
    m_executor.submit([this, stateProviderName, stateRequestToken] {
        executeProvideState(stateProviderName, true, stateRequestToken);
    });
}

void ExternalMediaPlayer::handleDirectiveImmediately(std::shared_ptr<AVSDirective> directive) {
    handleDirective(std::make_shared<DirectiveInfo>(directive, nullptr));
}

void ExternalMediaPlayer::preHandleDirective(std::shared_ptr<DirectiveInfo> info) {
}

bool ExternalMediaPlayer::parseDirectivePayload(std::shared_ptr<DirectiveInfo> info, rapidjson::Document* document) {
    rapidjson::ParseResult result = document->Parse(info->directive->getPayload());

    if (result) {
        return true;
    }

    ACSDK_ERROR(LX("parseDirectivePayloadFailed")
                    .d("reason", rapidjson::GetParseError_En(result.Code()))
                    .d("offset", result.Offset())
                    .d("messageId", info->directive->getMessageId()));

    sendExceptionEncounteredAndReportFailed(
        info, "Unable to parse payload", ExceptionErrorType::UNEXPECTED_INFORMATION_RECEIVED);

    return false;
}

void ExternalMediaPlayer::handleDirective(std::shared_ptr<DirectiveInfo> info) {
    if (!info) {
        ACSDK_ERROR(LX("handleDirectiveFailed").d("reason", "nullDirectiveInfo"));
        return;
    }

    NamespaceAndName directiveNamespaceAndName(info->directive->getNamespace(), info->directive->getName());
    auto handlerIt = m_directiveToHandlerMap.find(directiveNamespaceAndName);
    if (handlerIt == m_directiveToHandlerMap.end()) {
        ACSDK_ERROR(LX("handleDirectivesFailed")
                        .d("reason", "noDirectiveHandlerForDirective")
                        .d("nameSpace", info->directive->getNamespace())
                        .d("name", info->directive->getName()));
        sendExceptionEncounteredAndReportFailed(
            info, "Unhandled directive", ExceptionErrorType::UNEXPECTED_INFORMATION_RECEIVED);
        return;
    }

    ACSDK_DEBUG9(LX("handleDirectivesPayload").sensitive("Payload", info->directive->getPayload()));

    auto handler = (handlerIt->second.second);
    (this->*handler)(info, handlerIt->second.first);
}

std::shared_ptr<ExternalMediaAdapterInterface> ExternalMediaPlayer::preprocessDirective(
    std::shared_ptr<DirectiveInfo> info,
    rapidjson::Document* document) {
    ACSDK_DEBUG9(LX("preprocessDirective"));

    if (!parseDirectivePayload(info, document)) {
        return nullptr;
    }

    std::string playerId;
    if (!jsonUtils::retrieveValue(*document, PLAYER_ID, &playerId)) {
        ACSDK_ERROR(LX("preprocessDirectiveFailed").d("reason", "nullPlayerId"));
        sendExceptionEncounteredAndReportFailed(info, "No PlayerId in directive.");
        return nullptr;
    }

#ifdef EXTERNALMEDIAPLAYER_1_1
    if (m_adapters.empty()) { // use handlers when there are no adapters
        return nullptr;
    }
#endif

    auto adapterIt = m_adapters.find(playerId);
    if (adapterIt == m_adapters.end()) {
        ACSDK_ERROR(LX("preprocessDirectiveFailed").d("reason", "noAdapterForPlayerId").d(PLAYER_ID, playerId));
        sendExceptionEncounteredAndReportFailed(info, "Unrecogonized PlayerId.");
        return nullptr;
    }

    auto adapter = adapterIt->second;
    if (!adapter) {
        ACSDK_ERROR(LX("preprocessDirectiveFailed").d("reason", "nullAdapter").d(PLAYER_ID, playerId));
        sendExceptionEncounteredAndReportFailed(info, "nullAdapter.");
        return nullptr;
    }

    return adapter;
}

#ifdef EXTERNALMEDIAPLAYER_1_1
void ExternalMediaPlayer::handleAuthorizeDiscoveredPlayers(std::shared_ptr<DirectiveInfo> info, RequestType request) {
    rapidjson::Document payload;

    if (!parseDirectivePayload(info, &payload)) {
        return;
    }

    m_executor.submit([this, info]() {
        for (auto adapterHandler : m_adapterHandlers) {
            adapterHandler->authorizeDiscoveredPlayers(info->directive->getPayload());
        }
        setHandlingCompleted(info);
    });
}
#endif

void ExternalMediaPlayer::handleLogin(std::shared_ptr<DirectiveInfo> info, RequestType request) {
    rapidjson::Document payload;

    auto adapter = preprocessDirective(info, &payload);
    if (!adapter) {
#ifdef EXTERNALMEDIAPLAYER_1_1
        m_executor.submit([this, info]() {
            for (auto adapterHandler : m_adapterHandlers) {
                adapterHandler->login(info->directive->getPayload());
            }
            setHandlingCompleted(info);
        });
#endif
        return;
    }

    std::string accessToken;
    if (!jsonUtils::retrieveValue(payload, "accessToken", &accessToken)) {
        ACSDK_ERROR(LX("handleDirectiveFailed").d("reason", "nullAccessToken"));
        sendExceptionEncounteredAndReportFailed(info, "missing accessToken in Login directive");
        return;
    }

    std::string userName;
    if (!jsonUtils::retrieveValue(payload, USERNAME, &userName)) {
        userName = "";
    }

    int64_t refreshInterval;
    if (!jsonUtils::retrieveValue(payload, "tokenRefreshIntervalInMilliseconds", &refreshInterval)) {
        ACSDK_ERROR(LX("handleDirectiveFailed").d("reason", "nullRefreshInterval"));
        sendExceptionEncounteredAndReportFailed(info, "missing tokenRefreshIntervalInMilliseconds in Login directive");
        return;
    }

    bool forceLogin;
    if (!jsonUtils::retrieveValue(payload, "forceLogin", &forceLogin)) {
        ACSDK_ERROR(LX("handleDirectiveFailed").d("reason", "nullForceLogin"));
        sendExceptionEncounteredAndReportFailed(info, "missing forceLogin in Login directive");
        return;
    }

    setHandlingCompleted(info);
    adapter->handleLogin(accessToken, userName, forceLogin, std::chrono::milliseconds(refreshInterval));
}

void ExternalMediaPlayer::handleLogout(std::shared_ptr<DirectiveInfo> info, RequestType request) {
    rapidjson::Document payload;

    auto adapter = preprocessDirective(info, &payload);
    if (!adapter) {
#ifdef EXTERNALMEDIAPLAYER_1_1
        m_executor.submit([this, info]() {
            for (auto adapterHandler : m_adapterHandlers) {
                adapterHandler->logout(info->directive->getPayload());
            }
            setHandlingCompleted(info);
        });
#endif
        return;
    }

    setHandlingCompleted(info);
    adapter->handleLogout();
}

void ExternalMediaPlayer::handlePlay(std::shared_ptr<DirectiveInfo> info, RequestType request) {
    rapidjson::Document payload;

    auto adapter = preprocessDirective(info, &payload);
    if (!adapter) {
#ifdef EXTERNALMEDIAPLAYER_1_1
        setHaltInitiatorRequestHelper(request);
        m_executor.submit([this, info]() {
            for (auto adapterHandler : m_adapterHandlers) {
                adapterHandler->play(info->directive->getPayload());
            }
            setHandlingCompleted(info);
        });
#endif
        return;
    }

    std::string playbackContextToken;
    if (!jsonUtils::retrieveValue(payload, "playbackContextToken", &playbackContextToken)) {
        ACSDK_ERROR(LX("handleDirectiveFailed").d("reason", "nullPlaybackContextToken"));
        sendExceptionEncounteredAndReportFailed(info, "missing playbackContextToken in Play directive");
        return;
    }

    int64_t offset;
    if (!jsonUtils::retrieveValue(payload, "offsetInMilliseconds", &offset)) {
        offset = 0;
    }

    int64_t index;
    if (!jsonUtils::retrieveValue(payload, "index", &index)) {
        index = 0;
    }

#ifdef EXTERNALMEDIAPLAYER_1_1
    std::string skillToken;
    if (!jsonUtils::retrieveValue(payload, "skillToken", &skillToken)) {
        ACSDK_ERROR(LX("handleDirectiveFailed").d("reason", "nullSkillToken"));
        sendExceptionEncounteredAndReportFailed(info, "missing skillToken in Play directive");
        return;
    }

    std::string playbackSessionId;
    if (!jsonUtils::retrieveValue(payload, "playbackSessionId", &playbackSessionId)) {
        ACSDK_ERROR(LX("handleDirectiveFailed").d("reason", "nullPlaybackSessionId"));
        sendExceptionEncounteredAndReportFailed(info, "missing playbackSessionId in Play directive");
        return;
    }

    std::string navigation;
    if (!jsonUtils::retrieveValue(payload, "navigation", &navigation)) {
        ACSDK_ERROR(LX("handleDirectiveFailed").d("reason", "nullNavigation"));
        sendExceptionEncounteredAndReportFailed(info, "missing navigation in Play directive");
        return;
    }

    bool preload;
    if (!jsonUtils::retrieveValue(payload, "preload", &preload)) {
        ACSDK_ERROR(LX("handleDirectiveFailed").d("reason", "nullPreload"));
        sendExceptionEncounteredAndReportFailed(info, "missing preload in Play directive");
        return;
    }

    setHandlingCompleted(info);
    adapter->handlePlay(playbackContextToken, index, std::chrono::milliseconds(offset), skillToken, playbackSessionId, navigation, preload);
#else
    setHandlingCompleted(info);
    adapter->handlePlay(playbackContextToken, index, std::chrono::milliseconds(offset));
#endif
}

void ExternalMediaPlayer::handleSeek(std::shared_ptr<DirectiveInfo> info, RequestType request) {
    rapidjson::Document payload;

    auto adapter = preprocessDirective(info, &payload);
    if (!adapter) {
#ifdef EXTERNALMEDIAPLAYER_1_1
        m_executor.submit([this, info]() {
            for (auto adapterHandler : m_adapterHandlers) {
                adapterHandler->seek(info->directive->getPayload());
            }
            setHandlingCompleted(info);
        });
#endif
        return;
    }

    int64_t position;
    if (!jsonUtils::retrieveValue(payload, POSITIONINMS, &position)) {
        ACSDK_ERROR(LX("handleDirectiveFailed").d("reason", "nullPosition"));
        sendExceptionEncounteredAndReportFailed(info, "missing positionMilliseconds in SetSeekPosition directive");
        return;
    }

    setHandlingCompleted(info);
    adapter->handleSeek(std::chrono::milliseconds(position));
}

void ExternalMediaPlayer::handleAdjustSeek(std::shared_ptr<DirectiveInfo> info, RequestType request) {
    rapidjson::Document payload;

    auto adapter = preprocessDirective(info, &payload);
    if (!adapter) {
#ifdef EXTERNALMEDIAPLAYER_1_1
        m_executor.submit([this, info]() {
            for (auto adapterHandler : m_adapterHandlers) {
                adapterHandler->adjustSeek(info->directive->getPayload());
            }
            setHandlingCompleted(info);
        });
#endif
        return;
    }

    int64_t deltaPosition;
    if (!jsonUtils::retrieveValue(payload, "deltaPositionMilliseconds", &deltaPosition)) {
        ACSDK_ERROR(LX("handleDirectiveFailed").d("reason", "nullDeltaPositionMilliseconds"));
        sendExceptionEncounteredAndReportFailed(
            info, "missing deltaPositionMilliseconds in AdjustSeekPosition directive");
        return;
    }

    if (deltaPosition < MAX_PAST_OFFSET || deltaPosition > MAX_FUTURE_OFFSET) {
        ACSDK_ERROR(LX("handleDirectiveFailed").d("reason", "deltaPositionMillisecondsOutOfRange."));
        sendExceptionEncounteredAndReportFailed(
            info, "missing deltaPositionMilliseconds in AdjustSeekPosition directive");
        return;
    }

    setHandlingCompleted(info);
    adapter->handleAdjustSeek(std::chrono::milliseconds(deltaPosition));
}

void ExternalMediaPlayer::handlePlayControl(std::shared_ptr<DirectiveInfo> info, RequestType request) {
    rapidjson::Document payload;

    auto adapter = preprocessDirective(info, &payload);
    if (!adapter) {
#ifdef EXTERNALMEDIAPLAYER_1_1
        setHaltInitiatorRequestHelper(request);
        m_executor.submit([this, info, request]() {
            for (auto adapterHandler : m_adapterHandlers) {
                adapterHandler->playControl(info->directive->getPayload(),request);
            }
            setHandlingCompleted(info);
        });
#endif
        return;
    }

    setHandlingCompleted(info);
    adapter->handlePlayControl(request);
}

void ExternalMediaPlayer::cancelDirective(std::shared_ptr<DirectiveInfo> info) {
    removeDirective(info);
}

void ExternalMediaPlayer::onDeregistered() {
}

DirectiveHandlerConfiguration ExternalMediaPlayer::getConfiguration() const {
    return g_configuration;
}

#ifdef EXTERNALMEDIAPLAYER_1_1

void ExternalMediaPlayer::setCurrentActivity(const avsCommon::avs::PlayerActivity currentActivity) {
    ACSDK_DEBUG9(LX("setCurrentActivity").d("from", m_currentActivity).d("to", currentActivity));
    {
        std::lock_guard<std::mutex> lock(m_currentActivityMutex);
        m_currentActivity = currentActivity;
    }
    m_currentActivityConditionVariable.notify_all();
}

void ExternalMediaPlayer::setPlayerInFocus(const std::string& playerInFocus, bool focusAcquire) {
    ACSDK_DEBUG9(LX("setPlayerInFocus").d("playerInFocus", playerInFocus).d("focusAcquire", focusAcquire ? "true" : "false"));
    if (focusAcquire) {
        m_playerInFocus = playerInFocus;
        m_playbackRouter->setHandler(shared_from_this());
        // Acquire the channel and have this ExternalMediaPlayer manage the focus state.
        if (m_focus == FocusState::NONE && m_focusAcquireInProgress != true) {
            m_currentActivity = avsCommon::avs::PlayerActivity::IDLE;
            m_haltInitiator = HaltInitiator::NONE;
            m_focusAcquireInProgress = true;
            m_focusManager->acquireChannel(CHANNEL_NAME, shared_from_this(), FOCUS_MANAGER_ACTIVITY_ID);
        }
    }
    else if (playerInFocus.compare(m_playerInFocus) == 0 && m_focus != avsCommon::avs::FocusState::NONE) {
        // We only release the channel when the player is the player in focus.
        m_focusManager->releaseChannel(CHANNEL_NAME, shared_from_this());
    }
}

#endif

void ExternalMediaPlayer::setPlayerInFocus(const std::string& playerInFocus) {
    ACSDK_DEBUG9(LX("setPlayerInFocus").d("playerInFocus", playerInFocus));
    m_playerInFocus = playerInFocus;
    m_playbackRouter->setHandler(shared_from_this());
}

void ExternalMediaPlayer::onButtonPressed(PlaybackButton button) {
    auto buttonIt = g_buttonToRequestType.find(button);

    if (g_buttonToRequestType.end() == buttonIt) {
        ACSDK_ERROR(LX("ButtonToRequestTypeNotFound").d("button", button));
        return;
    }

    if (!m_playerInFocus.empty()) {
        auto adapterIt = m_adapters.find(m_playerInFocus);

        if (m_adapters.end() == adapterIt) {
#ifdef EXTERNALMEDIAPLAYER_1_1
            setHaltInitiatorRequestHelper(buttonIt->second);
            m_executor.submit([this, buttonIt]() {
                for (auto adapterHandler : m_adapterHandlers) {
                    adapterHandler->playControlForPlayer(m_playerInFocus, buttonIt->second);
                }
            });
#else
            // Should never reach here as playerInFocus is always set based on a contract with AVS.
            ACSDK_ERROR(LX("AdapterNotFound").d("player", m_playerInFocus));
#endif
            return;
        }

        adapterIt->second->handlePlayControl(buttonIt->second);
    }
}

void ExternalMediaPlayer::onTogglePressed(PlaybackToggle toggle, bool action) {
    auto toggleIt = g_toggleToRequestType.find(toggle);

    if (g_toggleToRequestType.end() == toggleIt) {
        ACSDK_ERROR(LX("ToggleToRequestTypeNotFound").d("toggle", toggle));
        return;
    }

    // toggleStates map is <SELECTED,DESELECTED>
    auto toggleStates = toggleIt->second;

    if (!m_playerInFocus.empty()) {
        auto adapterIt = m_adapters.find(m_playerInFocus);

        if (m_adapters.end() == adapterIt) {
#ifdef EXTERNALMEDIAPLAYER_1_1
            m_executor.submit([this, action, toggleStates]() {
                for (auto adapterHandler : m_adapterHandlers) {
                    if (action) {
                        adapterHandler->playControlForPlayer(m_playerInFocus, toggleStates.first);
                    }
                    else {
                        adapterHandler->playControlForPlayer(m_playerInFocus, toggleStates.second);
                    }
                }
            });
#else
            // Should never reach here as playerInFocus is always set based on a contract with AVS.
            ACSDK_ERROR(LX("AdapterNotFound").d("player", m_playerInFocus));
#endif
            return;
        }

        adapterIt->second->handlePlayControl(action ? toggleStates.first : toggleStates.second);
    }
}

void ExternalMediaPlayer::doShutdown() {
    m_executor.shutdown();
#ifdef EXTERNALMEDIAPLAYER_1_1
    m_adapterHandlers.clear();
    m_focusManager.reset();
#endif
     // Reset th
    // Reset the EMP from being a state provider. If not there would be calls from the adapter to provide context
    // which will try to add tasks to the executor thread.
    m_contextManager->setStateProvider(SESSION_STATE, nullptr);
    m_contextManager->setStateProvider(PLAYBACK_STATE, nullptr);

    for (auto& adapter : m_adapters) {
        if (!adapter.second) {
            continue;
        }
        adapter.second->shutdown();
    }

    m_adapters.clear();
    m_exceptionEncounteredSender.reset();
    m_contextManager.reset();
    m_playbackRouter.reset();
    m_speakerManager.reset();
}

void ExternalMediaPlayer::removeDirective(std::shared_ptr<DirectiveInfo> info) {
    // Check result too, to catch cases where DirectiveInfo was created locally, without a nullptr result.
    // In those cases there is no messageId to remove because no result was expected.
    if (info->directive && info->result) {
        CapabilityAgent::removeDirective(info->directive->getMessageId());
    }
}
#ifdef EXTERNALMEDIAPLAYER_1_1
void ExternalMediaPlayer::setHaltInitiatorRequestHelper(RequestType request) {
    switch (request) {
        case RequestType::PAUSE:
            m_haltInitiator = HaltInitiator::EXTERNAL_PAUSE;
            break;
        case RequestType::PAUSE_RESUME_TOGGLE:
            if (m_currentActivity == avsCommon::avs::PlayerActivity::PLAYING ||
                    (m_currentActivity == avsCommon::avs::PlayerActivity::PAUSED &&
                    m_haltInitiator == HaltInitiator::FOCUS_CHANGE_PAUSE)) {
                m_haltInitiator = HaltInitiator::EXTERNAL_PAUSE;
            }
            break;
        case RequestType::PLAY:
        case RequestType::RESUME:
            m_haltInitiator = HaltInitiator::NONE;
            break;
        default:
            break;
    }
}
#endif

void ExternalMediaPlayer::setHandlingCompleted(std::shared_ptr<DirectiveInfo> info) {
    if (info && info->result) {
        info->result->setCompleted();
    }

    removeDirective(info);
}

void ExternalMediaPlayer::sendExceptionEncounteredAndReportFailed(
    std::shared_ptr<DirectiveInfo> info,
    const std::string& message,
    avsCommon::avs::ExceptionErrorType type) {
    if (info && info->directive) {
        m_exceptionEncounteredSender->sendExceptionEncountered(info->directive->getUnparsedDirective(), type, message);
    }

    if (info && info->result) {
        info->result->setFailed(message);
    }

    removeDirective(info);
}

void ExternalMediaPlayer::executeProvideState(
    const avsCommon::avs::NamespaceAndName& stateProviderName,
    bool sendToken,
    unsigned int stateRequestToken) {
    ACSDK_DEBUG(LX("executeProvideState").d("sendToken", sendToken).d("stateRequestToken", stateRequestToken));
    std::string state;

#ifdef EXTERNALMEDIAPLAYER_1_1
    std::vector<avsCommon::sdkInterfaces::externalMediaPlayer::AdapterState> adapterStates;
    if (m_adapters.empty()) { // use handlers when there are no adapters
        for (auto adapterHandler : m_adapterHandlers) {
            auto handlerAdapterStates = adapterHandler->getAdapterStates();
            adapterStates.insert(adapterStates.end(), handlerAdapterStates.begin(), handlerAdapterStates.end());
        }
    }
#endif

    if (stateProviderName == SESSION_STATE) {
#ifdef EXTERNALMEDIAPLAYER_1_1
        state = provideSessionState(adapterStates);
#else
        state = provideSessionState();
#endif
    } else if (stateProviderName == PLAYBACK_STATE) {
#ifdef EXTERNALMEDIAPLAYER_1_1
        state = providePlaybackState(adapterStates);
#else
        state = providePlaybackState();
#endif
    } else {
        ACSDK_ERROR(LX("executeProvideState").d("reason", "unknownStateProviderName"));
        return;
    }

    SetStateResult result;
    if (sendToken) {
        result = m_contextManager->setState(stateProviderName, state, StateRefreshPolicy::ALWAYS, stateRequestToken);
    } else {
        result = m_contextManager->setState(stateProviderName, state, StateRefreshPolicy::ALWAYS);
    }

    if (result != SetStateResult::SUCCESS) {
        ACSDK_ERROR(LX("executeProvideState").d("reason", "contextManagerSetStateFailedForEMPState"));
    }
}

#ifdef EXTERNALMEDIAPLAYER_1_1
std::string ExternalMediaPlayer::provideSessionState(std::vector<avsCommon::sdkInterfaces::externalMediaPlayer::AdapterState> adapterStates) {
#else
std::string ExternalMediaPlayer::provideSessionState() {
#endif
    rapidjson::Document state(rapidjson::kObjectType);
    rapidjson::Document::AllocatorType& stateAlloc = state.GetAllocator();

    state.AddMember(rapidjson::StringRef(PLAYER_IN_FOCUS), m_playerInFocus, stateAlloc);
#ifdef EXTERNALMEDIAPLAYER_1_1
    state.AddMember(rapidjson::StringRef(SPI_VERSION), rapidjson::StringRef(SPI_VERSION_DEFAULT), stateAlloc);
    state.AddMember(rapidjson::StringRef(AGENT), rapidjson::StringRef(AGENT_DEFAULT), stateAlloc);
#endif
    rapidjson::Value players(rapidjson::kArrayType);
    for (const auto& adapter : m_adapters) {
        if (!adapter.second) {
            continue;
        }
        auto state = adapter.second->getState().sessionState;
        rapidjson::Value playerJson = buildSessionState(state, stateAlloc);
        players.PushBack(playerJson, stateAlloc);
        ObservableSessionProperties update{state.loggedIn, state.userName};
        notifyObservers(state.playerId, &update);
    }
    
#ifdef EXTERNALMEDIAPLAYER_1_1
    for (auto adapterState : adapterStates) {
        if (adapterState.sessionState.playerId.empty()) {
            continue;
        }
        rapidjson::Value playerJson = buildSessionState(adapterState.sessionState, stateAlloc);
        players.PushBack(playerJson, stateAlloc);
    }
#endif

    state.AddMember(rapidjson::StringRef(PLAYERS), players, stateAlloc);

    rapidjson::StringBuffer buffer;
    rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
    if (!state.Accept(writer)) {
        ACSDK_ERROR(LX("provideSessionStateFailed").d("reason", "writerRefusedJsonObject"));
        return "";
    }

    return buffer.GetString();
}

#ifdef EXTERNALMEDIAPLAYER_1_1
std::string ExternalMediaPlayer::providePlaybackState(std::vector<avsCommon::sdkInterfaces::externalMediaPlayer::AdapterState> adapterStates) {
#else
std::string ExternalMediaPlayer::providePlaybackState() {
#endif
    rapidjson::Document state(rapidjson::kObjectType);
    rapidjson::Document::AllocatorType& stateAlloc = state.GetAllocator();

    // Fill the default player state.
    if (!buildDefaultPlayerState(&state, stateAlloc)) {
        return "";
    }

    // Fetch actual PlaybackState from every player supported by the ExternalMediaPlayer.
    rapidjson::Value players(rapidjson::kArrayType);
    for (const auto& adapter : m_adapters) {
        if (!adapter.second) {
            continue;
        }
        auto state = adapter.second->getState().playbackState;
        rapidjson::Value playerJson = buildPlaybackState(state, stateAlloc);
        players.PushBack(playerJson, stateAlloc);
        ObservablePlaybackStateProperties update{state.state, state.trackName};
        notifyObservers(state.playerId, &update);
    }

#ifdef EXTERNALMEDIAPLAYER_1_1
    for (auto adapterState : adapterStates) {
        rapidjson::Value playerJson = buildPlaybackState(adapterState.playbackState, stateAlloc);
        players.PushBack(playerJson, stateAlloc);
    }
#endif

    state.AddMember(PLAYERS, players, stateAlloc);

    rapidjson::StringBuffer buffer;
    rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);

    if (!state.Accept(writer)) {
        ACSDK_ERROR(LX("providePlaybackState").d("reason", "writerRefusedJsonObject"));
        return "";
    }

    return buffer.GetString();
}

void ExternalMediaPlayer::createAdapters(
    const AdapterMediaPlayerMap& mediaPlayers,
    const AdapterSpeakerMap& speakers,
    const AdapterCreationMap& adapterCreationMap,
    std::shared_ptr<MessageSenderInterface> messageSender,
    std::shared_ptr<FocusManagerInterface> focusManager,
    std::shared_ptr<ContextManagerInterface> contextManager) {
    ACSDK_DEBUG0(LX("createAdapters"));
    for (auto& entry : adapterCreationMap) {
        auto mediaPlayerIt = mediaPlayers.find(entry.first);
        auto speakerIt = speakers.find(entry.first);

        if (mediaPlayerIt == mediaPlayers.end()) {
            ACSDK_ERROR(LX("adapterCreationFailed").d(PLAYER_ID, entry.first).d("reason", "nullMediaPlayer"));
            continue;
        }

        if (speakerIt == speakers.end()) {
            ACSDK_ERROR(LX("adapterCreationFailed").d("playerId", entry.first).d("reason", "nullSpeaker"));
            continue;
        }

        auto adapter = entry.second(
            (*mediaPlayerIt).second,
            (*speakerIt).second,
            m_speakerManager,
            messageSender,
            focusManager,
            contextManager,
            shared_from_this());
        if (adapter) {
            m_adapters[entry.first] = adapter;
        } else {
            ACSDK_ERROR(LX("adapterCreationFailed").d(PLAYER_ID, entry.first));
        }
    }
}

std::unordered_set<std::shared_ptr<avsCommon::avs::CapabilityConfiguration>> ExternalMediaPlayer::
    getCapabilityConfigurations() {
    return m_capabilityConfigurations;
}

void ExternalMediaPlayer::addObserver(std::shared_ptr<ExternalMediaPlayerObserverInterface> observer) {
    if (!observer) {
        ACSDK_ERROR(LX("addObserverFailed").d("reason", "nullObserver"));
        return;
    }
    std::lock_guard<std::mutex> lock{m_observersMutex};
    m_observers.insert(observer);
}

void ExternalMediaPlayer::removeObserver(std::shared_ptr<ExternalMediaPlayerObserverInterface> observer) {
    if (!observer) {
        ACSDK_ERROR(LX("removeObserverFailed").d("reason", "nullObserver"));
        return;
    }
    std::lock_guard<std::mutex> lock{m_observersMutex};
    m_observers.erase(observer);
}

void ExternalMediaPlayer::notifyObservers(
    const std::string& playerId,
    const ObservableSessionProperties* sessionProperties) {
    notifyObservers(playerId, sessionProperties, nullptr);
}

void ExternalMediaPlayer::notifyObservers(
    const std::string& playerId,
    const ObservablePlaybackStateProperties* playbackProperties) {
    notifyObservers(playerId, nullptr, playbackProperties);
}

void ExternalMediaPlayer::notifyObservers(
    const std::string& playerId,
    const ObservableSessionProperties* sessionProperties,
    const ObservablePlaybackStateProperties* playbackProperties) {
    if (playerId.empty()) {
        ACSDK_ERROR(LX("notifyObserversFailed").d("reason", "emptyPlayerId"));
        return;
    }

    std::unique_lock<std::mutex> lock{m_observersMutex};
    auto observers = m_observers;
    lock.unlock();

    for (auto& observer : observers) {
        if (sessionProperties) {
            observer->onLoginStateProvided(playerId, *sessionProperties);
        }

        if (playbackProperties) {
            observer->onPlaybackStateProvided(playerId, *playbackProperties);
        }
    }
}

}  // namespace externalMediaPlayer
}  // namespace capabilityAgents
}  // namespace alexaClientSDK
