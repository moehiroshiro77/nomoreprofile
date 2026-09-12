using System.Threading;

namespace PCL;

public static class ProfileUi
{
    public static bool CanCreateOtherProfile() => false;
}

internal static class Program
{
    private static void Main()
    {
        Console.WriteLine("target_started");
        Thread.Sleep(5000);
        Environment.ExitCode = ProfileUi.CanCreateOtherProfile() ? 42 : 7;
        Console.WriteLine(ProfileUi.CanCreateOtherProfile() ? "patched" : "original");
    }
}
