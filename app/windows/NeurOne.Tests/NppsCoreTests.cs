using NeurOne.Protocol;
using Xunit;

namespace NeurOne.Tests;

// The marshalling guards that answer before the native core is reached, so they run without
// neurone_npps_ffi.dll. The core's own behaviour is tested in common/npps-core and common/npps-ffi.
public class NppsCoreTests
{
    [Fact]
    public void ASessionUuidThatIsNot16BytesIsRefusedBeforeTheCore()
    {
        var options = new NppsCompileOptions { NowUnix = 0, SessionUuid = new byte[15] };
        var e = Assert.Throws<NppsRefusal>(() => NppsCore.Compile("{}"u8, options));
        Assert.Equal("sessionUUID must be 16 bytes", e.Message);
    }
}
