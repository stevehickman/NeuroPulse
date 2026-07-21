import { describe, it, expect } from 'vitest';
import {
  NP_SOCKET_COUNT,
  NP_SOCKET_ID_MAX,
  NP_SOCKET_ID_MIN,
  NP_SOCKET_NUMBERING_BASE,
  describeInvalid,
  socketIdProblem,
  socketRangeLabel,
  toSocketSet,
  unionSockets,
  unionZoneSockets,
} from './socketSet';

describe('socket id validation', () => {
  it('accepts every id on the helmet', () => {
    for (let id = NP_SOCKET_ID_MIN; id <= NP_SOCKET_ID_MAX; id++) {
      expect(socketIdProblem(id), `socket ${id}`).toBeUndefined();
    }
  });

  it('rejects ids outside the numbering base', () => {
    expect(socketIdProblem(0)).toBe('out-of-range');
    expect(socketIdProblem(-1)).toBe('out-of-range');
    expect(NP_SOCKET_NUMBERING_BASE).toBe(1);
  });

  it('rejects ids past the end of the lattice', () => {
    expect(socketIdProblem(NP_SOCKET_ID_MAX + 1)).toBe('out-of-range');
    expect(socketIdProblem(999)).toBe('out-of-range');
  });

  it('rejects non-integers', () => {
    expect(socketIdProblem(1.5)).toBe('not-an-integer');
    expect(socketIdProblem(NaN)).toBe('not-an-integer');
    expect(socketIdProblem(null)).toBe('not-an-integer');
    expect(socketIdProblem(undefined)).toBe('not-an-integer');
  });

  it('rejects values bare Number() would silently accept', () => {
    // Number(true)===1, Number([5])===5, Number('0x10')===16, Number('1e1')===10.
    // A socket id is a plain decimal integer or it is not a socket id.
    expect(socketIdProblem(true)).toBe('not-an-integer');
    expect(socketIdProblem([5])).toBe('not-an-integer');
    expect(socketIdProblem('0x10')).toBe('not-an-integer');
    expect(socketIdProblem('1e1')).toBe('not-an-integer');
    expect(socketIdProblem('')).toBe('not-an-integer');
  });

  it('agrees with toSocketSet on every input', () => {
    // Two exported validators disagreeing about the same value is a latent bug
    // for any caller that checks with one and canonicalises with the other.
    for (const v of [1, '1', '4', 0, -1, 1.5, NaN, null, true, [5], '0x10', '']) {
      const viaProblem = socketIdProblem(v) === undefined;
      const viaSet = toSocketSet([v]).sockets.length === 1;
      expect(viaSet, `disagreement on ${JSON.stringify(v)}`).toBe(viaProblem);
    }
  });

  it('labels the range for error messages', () => {
    expect(socketRangeLabel()).toBe(`${NP_SOCKET_ID_MIN}–${NP_SOCKET_ID_MAX}`);
  });
});

describe('toSocketSet', () => {
  it('deduplicates and sorts', () => {
    expect(toSocketSet([5, 3, 5, 1, 3]).sockets).toEqual([1, 3, 5]);
  });

  it('reports duplicates without dropping the underlying socket', () => {
    const r = toSocketSet([2, 2, 7]);
    expect(r.sockets).toEqual([2, 7]);
    expect(r.duplicates).toEqual([2]);
  });

  it('separates invalid ids from valid ones rather than throwing', () => {
    const r = toSocketSet([1, 0, 2, NP_SOCKET_ID_MAX + 1]);
    expect(r.sockets).toEqual([1, 2]);
    expect(r.invalid.map(e => e.raw)).toEqual([0, NP_SOCKET_ID_MAX + 1]);
    expect(r.invalid.map(e => e.problem)).toEqual(['out-of-range', 'out-of-range']);
  });

  it('deduplicates the invalid list too', () => {
    expect(toSocketSet([999, 999, 999]).invalid.map(e => e.raw)).toEqual([999]);
  });

  it('keeps distinct bad tokens distinct', () => {
    // Coercing first collapsed every unparseable token to one NaN, hiding all
    // but the first error in a file.
    const r = toSocketSet(['abc', 'xyz']);
    expect(r.invalid.map(e => describeInvalid(e))).toEqual(['"abc"', '"xyz"']);
  });

  it('preserves the original token for the error message', () => {
    expect(describeInvalid(toSocketSet(['banana']).invalid[0])).toBe('"banana"');
    expect(describeInvalid(toSocketSet([999]).invalid[0])).toBe('999');
  });

  it('is total — empty input yields empty results, never a throw', () => {
    expect(toSocketSet([])).toEqual({ sockets: [], invalid: [], duplicates: [] });
  });

  it('accepts decimal-integer strings so text input shares one path', () => {
    expect(toSocketSet(['3', ' 1 ', '3']).sockets).toEqual([1, 3]);
  });
});

describe('unionSockets', () => {
  it('counts a socket shared by two lists once', () => {
    // This is the midline case: a socket in both hemisphere zones of its lobe.
    expect(unionSockets([1, 2, 3], [3, 4])).toEqual([1, 2, 3, 4]);
  });

  it('is order-independent and always sorted', () => {
    expect(unionSockets([9, 1], [5])).toEqual(unionSockets([5], [1, 9]));
    expect(unionSockets([9, 1], [5])).toEqual([1, 5, 9]);
  });

  it('accepts Sets as readily as arrays', () => {
    expect(unionSockets(new Set([3, 1]), [1])).toEqual([1, 3]);
  });

  it('unioning a list with itself is idempotent', () => {
    const a = [4, 8, 15];
    expect(unionSockets(a, a)).toEqual(unionSockets(a));
  });

  it('never exceeds the socket count no matter how many lists overlap', () => {
    const all = Array.from({ length: NP_SOCKET_COUNT }, (_, i) => i + NP_SOCKET_ID_MIN);
    expect(unionSockets(all, all, all, all).length).toBe(NP_SOCKET_COUNT);
  });
});

describe('unionZoneSockets', () => {
  it('unions zone definitions and dedups the shared sockets', () => {
    const left = { sockets: [1, 2, 4, 5] };
    const right = { sockets: [2, 3, 6, 7] }; // socket 2 is midline, in both
    expect(unionZoneSockets([left, right])).toEqual([1, 2, 3, 4, 5, 6, 7]);
    expect(unionZoneSockets([left, right]).length).toBeLessThan(
      left.sockets.length + right.sockets.length,
    );
  });

  it('handles an empty zone list', () => {
    expect(unionZoneSockets([])).toEqual([]);
  });
});
