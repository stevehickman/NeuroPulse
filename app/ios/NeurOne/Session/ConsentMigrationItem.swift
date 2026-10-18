//  ConsentMigrationItem.swift
//
//  The consent item of a bundle, for the host of a migration flow (`MigrationHost`): the withdrawal records, and
//  nothing else. Grants are never carried (ruling, issue #569): a restore can only reduce what a phone is consented
//  to. The shape is the core's `withdrawalState`, the same one the Kotlin apps' `StoreConsentMigrationAdapter` reads
//  and writes, so a bundle moves between them. The later-time comparison is the core's (`applyWithdrawal`); this
//  only hands the store's records to it and the merged ones back (SWR-MG-04).

import Foundation

@MainActor
struct ConsentMigrationItem {
    /// The item's name in a bundle (`MIGRATION_ITEM_WITHDRAWAL_STATE`).
    static let name = "withdrawalState"

    let consent: ConsentStore

    /// What goes into a bundle.
    func export() -> [String: Any] { consent.exportWithdrawals() }

    /// What a restore brings. A withdrawal later than the one held switches the matching grant off; nothing turns one on.
    func restore(_ value: [String: Any]) throws { try consent.applyWithdrawals(value) }
}
