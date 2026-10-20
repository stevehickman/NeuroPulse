// Save and open the person's protocols as one .npps file. The app keeps nothing in the browser (appStorage.ts), so this file is
// how a person keeps their work from one visit to the next (REQ-SW03-18 (b)).
//
// What the file carries is the protocols and composites as the .npps format writes them. What it cannot carry is the library's
// bookkeeping, which is not in that format: which families have been used (SWR-WO-02), which the owner archived (SWR-WO-01) and
// which protocol was cloned from which (SWR-PL-06). A protocol opened from a file starts as unused and offered, so a family
// that was used and locked before the reload can be edited in place afterwards. That is the cost of keeping nothing in the browser.

import { serializeNPPS } from '../../../../common/lib/nppsSerializer';
import { parseNPPS } from '../../../../common/lib/nppsParser';
import type { ProtocolLibrary } from './protocolLibrary';

export const BACKUP_FILE_NAME = 'neurone-protocols.npps';

/** The user's protocols as .npps text, or null when there are none to save. Throws if one cannot be written. */
export function backupText(library: Pick<ProtocolLibrary, 'userProtocols'>): string | null {
  const mine = library.userProtocols;
  return mine.length === 0 ? null : serializeNPPS(mine);
}

export interface OpenResult {
  /** Protocols saved into the library. */
  opened: number;
  /** Protocols the library's rules refused (a read-only protocol, a used family, a variant that exists). */
  refused: number;
  /** The text was not a readable .npps file; nothing was changed. */
  unreadable: boolean;
}

/** Open protocols from .npps text. A file that does not parse changes nothing. */
export function openBackup(library: Pick<ProtocolLibrary, 'saveUserProtocol'>, text: string): OpenResult {
  let entries;
  try {
    entries = parseNPPS(text);
  } catch {
    return { opened: 0, refused: 0, unreadable: true };
  }
  let opened = 0;
  let refused = 0;
  for (const entry of entries) {
    if (library.saveUserProtocol(entry).kind === 'saved') opened++;
    else refused++;
  }
  return { opened, refused, unreadable: false };
}
