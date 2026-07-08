import NeurOneArgon2
import Foundation

public enum Argon2Error: Error {
    case derivationFailed
    case invalidParameters
}

// Derives 64 bytes (K1 + K2 for AES-256-XTS) using Argon2id.
// Fixed parameters per NeurOne UHDR spec (NP-FW-EMMC-001 Rev A §6):
//   memory=65536 KiB (64 MB), iterations=4, parallelism=1.
public func npArgon2idDeriveKey(password: Data, salt: Data, outputLength: Int = 64) throws -> Data {
    guard !password.isEmpty, !salt.isEmpty, outputLength > 0 else {
        throw Argon2Error.invalidParameters
    }
    var output = Data(count: outputLength)
    let rc: Int32 = output.withUnsafeMutableBytes { outPtr in
        password.withUnsafeBytes { pwdPtr in
            salt.withUnsafeBytes { saltPtr in
                np_argon2id_hash(
                    pwdPtr.baseAddress?.assumingMemoryBound(to: UInt8.self),
                    UInt32(password.count),
                    saltPtr.baseAddress?.assumingMemoryBound(to: UInt8.self),
                    UInt32(salt.count),
                    65536,
                    4,
                    outPtr.baseAddress?.assumingMemoryBound(to: UInt8.self),
                    UInt32(outputLength)
                )
            }
        }
    }
    guard rc == 0 else { throw Argon2Error.derivationFailed }
    return output
}

#if DEBUG
// Configurable parameters for unit tests — avoids 64 MB allocation in CI.
// Minimum memory_kib=8 (Argon2 spec minimum for p=1).
public func npArgon2idDeriveKeyTestOnly(
    password: Data,
    salt: Data,
    memoryKib: UInt32 = 64,
    iterations: UInt32 = 1,
    outputLength: Int = 64
) throws -> Data {
    guard !password.isEmpty, !salt.isEmpty, outputLength > 0 else {
        throw Argon2Error.invalidParameters
    }
    var output = Data(count: outputLength)
    let rc: Int32 = output.withUnsafeMutableBytes { outPtr in
        password.withUnsafeBytes { pwdPtr in
            salt.withUnsafeBytes { saltPtr in
                np_argon2id_hash_custom(
                    pwdPtr.baseAddress?.assumingMemoryBound(to: UInt8.self),
                    UInt32(password.count),
                    saltPtr.baseAddress?.assumingMemoryBound(to: UInt8.self),
                    UInt32(salt.count),
                    memoryKib,
                    iterations,
                    outPtr.baseAddress?.assumingMemoryBound(to: UInt8.self),
                    UInt32(outputLength)
                )
            }
        }
    }
    guard rc == 0 else { throw Argon2Error.derivationFailed }
    return output
}
#endif
