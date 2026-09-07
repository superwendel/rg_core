Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
$repo = Split-Path -Parent $PSScriptRoot
$output = Join-Path $repo '.test-build\includes'
New-Item -ItemType Directory -Path $output -Force | Out-Null
$cases = @(
    @{ Order=0; Asm=0; Flags=@() },
    @{ Order=1; Asm=1; Flags=@() },
    @{ Order=2; Asm=1; Flags=@() },
    @{ Order=3; Asm=1; Flags=@() },
    @{ Order=1; Asm=0; Flags=@('/DRG_SPRINTF_HYBRID_FORCE_C') },
    @{ Order=3; Asm=0; Flags=@('/DRG_SPRINTF_NO_ASM') },
    @{ Order=1; Asm=1; Flags=@('/DRG_SPRINTF_HYBRID_FORCE_ASM', '/DRG_SPRINTF_HAS_ASM') },
    @{ Order=1; Asm=0; Flags=@('/DRG_SPRINTF_HYBRID_FORCE_C', '/DRG_SPRINTF_HYBRID_FORCE_ASM', '/DRG_SPRINTF_HAS_ASM') },
    @{ Order=3; Asm=0; Flags=@('/DRG_SPRINTF_NO_ASM', '/DRG_SPRINTF_HYBRID_FORCE_ASM', '/DRG_SPRINTF_HAS_ASM') },
    @{ Order=2; Asm=1; Flags=@('/DRG_SPRINTF_NO_ASM') }
)
for ($i = 0; $i -lt $cases.Count; ++$i) {
    $case = $cases[$i]
    $exe = Join-Path $output "include_c_$i.exe"
    $obj = Join-Path $output "include_c_$i.obj"
    $arguments = @('/nologo', '/std:c11', '/W4', '/WX', '/wd4505', '/O2', '/arch:AVX2',
        '/D_CRT_SECURE_NO_WARNINGS', "/DRG_SPRINTF_INCLUDE_ORDER=$($case.Order)",
        "/DRG_SPRINTF_EXPECT_ASM=$($case.Asm)", "/Fo:$obj", "/Fe:$exe")
    $arguments += $case.Flags
    $arguments += Join-Path $repo 'tests\test_sprintf_includes.c'
    if ($case.Asm -and '/DRG_SPRINTF_NO_ASM' -notin $case.Flags) {
        $arguments += @('/link', (Join-Path $repo 'rg_sprintf_asm_x64.obj'))
    }
    & cl @arguments
    if ($LASTEXITCODE -ne 0) { throw "Include test C/$i failed to build." }
    & $exe
    if ($LASTEXITCODE -ne 0) { throw "Include test C/$i failed." }
}
Write-Output 'All 10 C formatter include/configuration builds passed.'
