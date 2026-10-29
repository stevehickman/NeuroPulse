package life.neurone.core.cloud

/** The query string of a shared access signature (without the `?`), read at every call so a signature replaced in the account is used by a store made before. */
fun interface AzureSasProvider {
    suspend fun query(): String
}
