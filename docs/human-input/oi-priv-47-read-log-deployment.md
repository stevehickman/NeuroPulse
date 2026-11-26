# OI-PRIV-47: three deployment choices for the analytics read log

**Status:** DRAFT, put to the user in the session of 2026-11-26 · **From:** the session working `OI-PRIV-47` · **To:** the principal (or whoever the principal names to run the analytics stack)
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

## Q2. Which cloud provider holds the locked bucket?

The log needs write-once storage with a deletion lock that the Privacy Lead owns, in an EU region where EU users are in scope. All three providers below offer that. The policy names all three.

| Option | Lock | Notes |
|---|---|---|
| A. AWS S3, Object Lock in compliance mode | Retention of 12 months per object. Compliance mode means nobody can delete or shorten it, root included | CloudTrail data events give A9 (lock, retention or lifecycle changed, deletion attempted) |
| B. Google Cloud Storage, a locked bucket retention policy | A locked policy can be lengthened but never shortened or removed | Cloud Audit Logs give A9 |
| C. Azure Blob, a locked time-based immutability policy | The same guarantee | Activity Log gives A9 |

**Recommendation:** the provider the company already holds an account and a data-processing agreement with, so that no new sub-processor is added. If there is none, A. The deciding fact is which agreement already exists, and the session does not know that.

The bucket must sit in **a separate account or project from the analytics host**, with the Privacy Lead as its only administrator. Otherwise whoever runs the analytics host could change the lock, and A9 would be the only thing that noticed.

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
