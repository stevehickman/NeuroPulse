package com.neurone.core.analytics

// Analytics backend abstraction — port of iOS AnalyticsBackend protocol.
// The concrete production backend (PostHog Android SDK) lives in :app;
// call sites use ResearchAnalyticsGate exclusively and never the SDK directly.

interface AnalyticsBackend {
    fun configure()
    fun reset()
    fun track(event: String, properties: Map<String, String>)
}
