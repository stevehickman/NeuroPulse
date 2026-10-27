package life.neurone.app

import life.neurone.core.net.HttpTransport
import life.neurone.core.net.HttpTransportChecks
import kotlin.test.Test

/** Android's transport (OkHttp) against the shared checks of what it does with the method it is handed (SWR-MG-11). */
class OkHttpTransportTest : HttpTransportChecks() {
    override fun transport(): HttpTransport = OkHttpTransport()

    // @verifies SWR-MG-11#calls
    @Test fun delete() = deleteIsSentAsDeleteWithNoBody()

    // @verifies SWR-MG-11#calls
    @Test fun put() = putCarriesItsBodyAndContentType()

    // @verifies SWR-MG-11#calls
    @Test fun post() = theDefaultMethodIsStillPostWithItsBody()

    // @verifies SWR-MG-11#calls
    @Test fun getAndHead() = getAndHeadCarryNoBody()

    // @verifies SWR-MG-11#calls
    @Test fun refusal() = aRefusalIsAnAnswerWithItsStatusAndHeaders()

    // @verifies SWR-MG-11#calls
    @Test fun noServer() = noServerGivesNullNotAnException()
}
