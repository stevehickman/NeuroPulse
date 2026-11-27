# OI-PRIV-47: three deployment choices for the analytics read log

**Status:** PARTLY ANSWERED 2026-11-27 by the user (no cloud or identity provider exists yet): Q1 is Amazon Cognito behind `oauth2-proxy`, and Q2 is AWS, both as working assumptions. Q3 remains open. First put to the user in the session of 2026-11-26 · **From:** the session working `OI-PRIV-47` · **To:** the principal (or whoever the principal names to run the analytics stack)
**GitHub issue:** [#970](https://github.com/stevehickman/NeuroPulse/issues/970)

## What is already decided

The principal decided the read-access policy on 2026-11-24 (`NP-ANALYTICS-001` §7.7). Only the Analytics Lead holds a PostHog account, with the CEO's approval. Every read is recorded: PostHog reads by a sign-in proxy, ClickHouse reads per user. The log goes to write-once storage that the Privacy Lead owns, in an EU region wherever EU users are in scope, and is kept for 12 months. Alerts A1 to A11 fire on every occurrence and are routed as decided.

What is built (2026-11-26): the retention script's own ClickHouse user, and the log's entry format and alert rules as a module that nothing runs yet (`NP-ANALYTICS-001` §7.8). The proxy, the forwarder, the bucket and the alert sender cannot be built until three products are chosen. These are deployment choices. The policy does not settle them, and choosing them is not the engineering session's call.

## Q1. Which identity provider stands behind the sign-in proxy?

The proxy signs a person in before any PostHog page loads, and it records who they are. Self-hosted PostHog's own login cannot be the record: all of PostHog's queries reach ClickHouse as one user, and PostHog records changes, not page views (still to be confirmed for the pinned version).

| Option | What it is | For | Against |
|---|---|---|---|
| **A. The company's existing workspace identity** (Google Workspace or Microsoft Entra ID) through `oauth2-proxy` | An open-source proxy (MIT licence) that signs in against the directory the company already runs | No new account system. Leavers lose access when their company account goes. The provider's sign-in log covers B2 (unusual time or device) | The proxy records the page; the provider records only the sign-in |
| B. A self-hosted provider (Keycloak, Authentik) | An identity server run beside PostHog | Nothing leaves the company's hosts | One more service to run, patch and back up, with its own accounts to keep in step with leavers |
| C. A cloud zero-trust gateway (Cloudflare Access and similar) | The provider's edge signs the person in and can log the request | Little to run | Every page path passes through a third party, so the redaction would have to run there as well. That puts a sub-processor in the read path |

**Recommendation: A**, whichever directory the company already uses. Once the proxy is built, the `who` field is that directory's account name.

**Answer (the user, 2026-11-27): Amazon Cognito** (a user pool behind `oauth2-proxy`, self sign-up off, multi-factor sign-in required), recorded in `NP-ANALYTICS-001` §7.8.

**Note (2026-11-27): the company has no identity provider either**, so option A has no directory to use. With AWS assumed, the proposal is an Amazon Cognito user pool with multi-factor sign-in required, behind `oauth2-proxy`. AWS IAM Identity Center is the alternative. It suits staff who also sign in to the AWS console, but it is less direct as an OIDC provider for a proxy. Whichever is chosen, `oauth2-proxy`'s own request log must be off: it writes the raw path, ID included, and the page record must come from the read-log module's redaction instead.

## Q2. Which cloud provider holds the locked bucket?

The log needs write-once storage with a deletion lock that the Privacy Lead owns, in an EU region where EU users are in scope. All three providers below offer that. The policy names all three.

| Option | Lock | Notes |
|---|---|---|
| A. AWS S3, Object Lock in compliance mode | Retention of 12 months per object. Compliance mode means nobody can delete or shorten it, root included | CloudTrail data events give A9 (lock, retention or lifecycle changed, deletion attempted) |
| B. Google Cloud Storage, a locked bucket retention policy | A locked policy can be lengthened but never shortened or removed | Cloud Audit Logs give A9 |
| C. Azure Blob, a locked time-based immutability policy | The same guarantee | Activity Log gives A9 |

**Recommendation:** the provider the company already holds an account and a data-processing agreement with, so that no new sub-processor is added. If there is none, A. The deciding fact is which agreement already exists, and the session does not know that.

The bucket must sit in **a separate account or project from the analytics host**, with the Privacy Lead as its only administrator. Otherwise whoever runs the analytics host could change the lock, and A9 would be the only thing that noticed.

**Answer (the user, 2026-11-27): assume AWS for now (option A).** It is recorded in `NP-ANALYTICS-001` §7.8 as a working assumption, and may be revisited before anything is deployed.

**Note (2026-11-27): the company has no cloud provider yet**, so no existing agreement decides Q2. It then follows from Q1. With Microsoft 365 or Entra, choose C: the Privacy Lead administers the bucket with the account they already have. With Google Workspace, choose B for the same reason. With neither, or where the fullest A9 coverage matters most, choose A in Frankfurt or Ireland. At this log's volume, storage costs a few dollars a month or less on all three. All three are US companies and so reachable under the US CLOUD Act. The log holds no analytics ID, but it does hold staff account names. EU-owned providers with S3-compatible Object Lock (Scaleway, OVHcloud) avoid that exposure. Their audit trail for bucket-setting changes is thinner, though, and §7.7 names only S3, GCS and Azure, so choosing one would mean amending §7.7.

**Note (2026-11-27): ClickHouse Cloud is not an option for Q2.** It is a managed database, not write-once storage. An administrator can always alter, truncate or drop a table, and no lock equivalent to S3 compliance mode exists. So it cannot meet §7.7's write-once, deletion-locked, Privacy-Lead-owned store, and the log would still need a bucket from one of the providers above. Hosting the *analytics store itself* on ClickHouse Cloud is a separate question for the principal, not a deployment detail:

- **For:** no ClickHouse to run, patch, back up or size; EU regions; a built-in query log of every user's queries (useful for the read log's ClickHouse side); per-user SQL grants.
- **Against:**
  - It reintroduces the third-party processor that self-hosting was chosen to remove (`NP-ANALYTICS-001` §2 and its closing note; `NP-PRIV-REM-001` STEP-05). That needs a data-processing agreement covering sub-processors, and may change the privacy notice and `NP-APP-TELEMETRY-001` §2.
  - The service still runs on AWS, GCP or Azure underneath.
  - ClickHouse Inc. is a US company.
  - It is not confirmed that self-hosted PostHog supports an external managed ClickHouse, since it creates its own Kafka-fed tables. This must be checked against the pinned `POSTHOG_VERSION`.
  - It would change how `np_erase` and `np_retention` are defined (SQL grants instead of `users.d` files).
  - It cuts across the `infra/posthog` rebuild (`OI-ANALYTICS-03`).

**Recommendation:** keep the analytics store self-hosted as the documents assume, and choose the bucket provider as above.

## Q3. Which channel do the alerts go to?

Every alert goes to the Privacy Lead. A1, A2, A4, A8, A9 and A11 go at once, and the rest within the week. The CEO is copied on A9 and on any alert caused by the Privacy Lead's own account.

| Option | For | Against |
|---|---|---|
| **A. Email to a role address** (privacy-lead@, ceo@) | Works with any provider. Leaves a record the review records can cite. A role address survives a change of person | Slow to be seen out of hours |
| B. A chat channel (Slack, Teams) | Seen quickly | Usually readable by more people than the Privacy Lead and the CEO, so the alert text must carry nothing but the rule and the entry's sequence number. Retention is the chat tool's own |
| C. A paging service (PagerDuty, Opsgenie) for the immediate ones, with email for the rest | Wakes someone for A8 and A9 | One more vendor, and someone has to be on call |

**Recommendation: A** for every alert. Add C for the immediate six only if someone is actually on call to answer it. Today the alert text is the rule, the source, the identity, the redacted object, the time and the sequence number. It holds no analytics ID, so it can go to any of the three.

## What to send back

For each question: the option (or another), the reason in a line, and who decided and when. For Q2, also the account the bucket will live in and who administers it. Once the answers are recorded in `NP-ANALYTICS-001` §7.8 and logged, the proxy, the forwarder and the alert sender can be built on `infra/posthog/read_log.py`.
