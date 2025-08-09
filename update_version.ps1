# Run dotnet-gitversion and parse the JSON output
$GitVersion = dotnet-gitversion | ConvertFrom-Json

# Extract version components
$Major = $GitVersion.Major
$Minor = $GitVersion.Minor
$Patch = $GitVersion.Patch
$SemVer = $GitVersion.SemVer
$CopyrightYear = (Get-Date).Year

# Format the VERSION_NUMBER and VERSION_STRING
$VersionNumber = "$Major,$Minor,$Patch,0"
$VersionString = "$Major.$Minor.$Patch.0"

# Define the updated version.h content
$HeaderContent = @"
#define VLDVERSION          L"$SemVer"
#define VERSION_NUMBER		$VersionNumber
#define VERSION_STRING		"$VersionString"
#define VERSION_COPYRIGHT	"Copyright (C) 2005-$CopyrightYear"

#ifndef __FILE__
!define VLD_VERSION "$SemVer"	// NSIS Script
#endif
"@

# Update the version.h file
$HeaderFilePath = "setup\version.h"
Set-Content -Path $HeaderFilePath -Value $HeaderContent

Write-Host "version.h updated successfully."
