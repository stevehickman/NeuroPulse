// RegionHelperTests.swift
//
// RegionHelper no longer contains any logic — locale-based Illinois detection
// was removed when the biometric consent disclosure was made universal.
// (OI-PA-03 resolved; see RegionHelper.swift for the full explanation.)
//
// The previous tests verified that Locale.current.region?.identifier == "US-IL"
// correctly identified Illinois. Those tests are deleted because the predicate
// they tested is gone.
