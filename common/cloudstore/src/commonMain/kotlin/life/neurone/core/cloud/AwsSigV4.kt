package life.neurone.core.cloud

/**
 * The person's own AWS credentials: an access key and its secret, and a session token when they are temporary (from
 * STS, IAM Identity Center or Cognito). NeurOne holds none; the host keeps them and hands them over per call (SWR-MG-11).
 */
class AwsCredentials(val accessKeyId: String, val secretAccessKey: String, val sessionToken: String? = null)

/** Called before every request, so a host can refresh temporary credentials lazily. */
fun interface AwsCredentialsProvider {
    suspend fun credentials(): AwsCredentials
}

/**
 * One request, described for signing. [path] is the path **already percent-encoded**, as it goes on the wire; [query] is
 * the raw `(name, value)` pairs, which the signer encodes and sorts; [headers] are the extra headers the caller will send and
 * wants signed; [payloadHash] is 64 lowercase hex digits or `UNSIGNED-PAYLOAD`; [amzDate] is `YYYYMMDDTHHMMSSZ` in UTC.
 */
class AwsSigningRequest(
    val method: String,
    val host: String,
    val path: String,
    val query: List<Pair<String, String>>,
    val headers: Map<String, String>,
    val payloadHash: String,
    val region: String,
    val service: String,
    val credentials: AwsCredentials,
    val amzDate: String,
)

/**
 * AWS Signature Version 4. It is an HMAC-SHA-256 chain, so it is the Rust core's (`CLAUDE.md` §22) and not written here:
 * `life.neurone.core.cloud.NppsAwsSigV4` in `:core` calls it, and `common/` imports nothing outside `common/`. Returns the
 * headers to add to the request: `x-amz-date`, `x-amz-content-sha256`, `x-amz-security-token` (when a token was given) and
 * `authorization`.
 */
fun interface AwsSigV4 {
    fun sign(request: AwsSigningRequest): Map<String, String>
}
