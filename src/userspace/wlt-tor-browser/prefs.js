// Tor Browser Privacy Configuration for WLTIOS
// Generated 2026-06-22

// Disable JavaScript by default
user_pref("javascript.enabled", false);

// Disable WebRTC (prevents IP leaks)
user_pref("media.peerconnection.enabled", false);
user_pref("media.peerconnection.turn.disable", true);
user_pref("media.peerconnection.use_document_iceservers", false);
user_pref("media.peerconnection.video.enabled", false);
user_pref("media.peerconnection.identity.timeout", 1);

// Disable WebGL (fingerprinting vector)
user_pref("webgl.disabled", true);
user_pref("webgl.enable-webgl2", false);

// Disable telemetry
user_pref("datareporting.policy.dataSubmissionEnabled", false);
user_pref("datareporting.healthreport.uploadEnabled", false);
user_pref("toolkit.telemetry.enabled", false);
user_pref("toolkit.telemetry.unified", false);

// Disable crash reports
user_pref("browser.crashReports.unsubmittedCheck.enabled", false);
user_pref("browser.crashReports.unsubmittedCheck.autoSubmit2", false);

// Disable pocket, screenshots, etc.
user_pref("extensions.pocket.enabled", false);
user_pref("extensions.screenshots.disabled", true);
user_pref("extensions.screenshots.upload-disabled", true);

// Privacy settings
user_pref("privacy.resistFingerprinting", true);
user_pref("privacy.trackingprotection.enabled", true);
user_pref("privacy.trackingprotection.fingerprinting.enabled", true);
user_pref("privacy.trackingprotection.cryptomining.enabled", true);
user_pref("privacy.donottrackheader.enabled", true);

// Disable disk cache (amnesic mode)
user_pref("browser.cache.disk.enable", false);
user_pref("browser.cache.memory.enable", true);
user_pref("browser.sessionstore.enabled", false);
user_pref("browser.formfill.enable", false);

// Tor SOCKS proxy
user_pref("network.proxy.socks", "127.0.0.1");
user_pref("network.proxy.socks_port", 9050);
user_pref("network.proxy.socks_remote_dns", true);
user_pref("network.proxy.type", 1);

// Disable auto-update (updates go through WLTIOS package manager)
user_pref("app.update.enabled", false);
user_pref("app.update.auto", false);

// Homepage
user_pref("browser.startup.homepage", "about:tor");
user_pref("browser.startup.page", 1);
