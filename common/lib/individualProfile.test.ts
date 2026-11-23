/**
 * What a stored individual profile is reduced to (NP-SW-001 REQ-SW03-32, OI-PRIV-11): the fields a profile has, with a
 * note inside its bound. The Kotlin store's own tests are NPLimitsStoreTests.
 */
import { describe, expect, it } from 'vitest';
import { profileNotesFit, storedProfile } from '../types/limits';
import { ProfileLimits } from './constants.generated';

const MAX = ProfileLimits.PROFILE_NOTES_MAX_CHARS;

describe('storedProfile', () => {
  // @verifies REQ-SW03-32#a
  it('drops a field a profile does not have', () => {
    const p = storedProfile({ id: 'p1', name: 'Alex', notes: 'left chair', dateOfBirth: '1980-01-01', createdAt: '2026-01-01T00:00:00Z', diagnosis: 'x' });
    expect(p).toEqual({ id: 'p1', name: 'Alex', notes: 'left chair' });
    expect(JSON.stringify(p)).not.toContain('1980');
  });

  // @verifies REQ-SW03-32#a
  it('is null for something that is not a profile', () => {
    expect(storedProfile(null)).toBeNull();
    expect(storedProfile('Alex')).toBeNull();
    expect(storedProfile({ name: 'Alex' })).toBeNull();
    expect(storedProfile({ id: 'p1', name: '' })).toBeNull();
  });

  // @verifies REQ-SW03-32#b
  it('a note at the bound fits and one past it does not', () => {
    expect(profileNotesFit('x'.repeat(MAX))).toBe(true);
    expect(profileNotesFit('x'.repeat(MAX + 1))).toBe(false);
  });

  // @verifies REQ-SW03-32#c
  it('cuts a stored note to the bound', () => {
    expect(storedProfile({ id: 'p1', name: 'Alex', notes: 'n'.repeat(MAX + 40) })?.notes).toBe('n'.repeat(MAX));
  });

  // @verifies REQ-SW03-32#d
  it('keeps the stealth choice, and only as true', () => {
    expect(storedProfile({ id: 'p1', name: 'Alex', notes: '', stealth: true })).toEqual({ id: 'p1', name: 'Alex', notes: '', stealth: true });
    expect(storedProfile({ id: 'p1', name: 'Alex', notes: '', stealth: false })).toEqual({ id: 'p1', name: 'Alex', notes: '' });
    expect(storedProfile({ id: 'p1', name: 'Alex', notes: '', stealth: 'yes' })).toEqual({ id: 'p1', name: 'Alex', notes: '' });
  });
});
