using System.Threading;

namespace PCL;

public static class ModProfile
{
    private static bool _GetAvailableProfileSelection(bool includeOfflineAndThirdParty) =>
        includeOfflineAndThirdParty;

    public static bool TestSelection() => _GetAvailableProfileSelection(false);
}

internal static class Program
{
    private static void Main()
    {
        Console.WriteLine("legacy_target_started");
        Thread.Sleep(5000);
        Console.WriteLine(ModProfile.TestSelection() ? "patched" : "original");
    }
}
