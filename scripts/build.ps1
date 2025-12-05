param()
$ErrorActionPreference = 'Stop'

$ProjectRoot = Split-Path -Parent $MyInvocation.MyCommand.Path | Split-Path -Parent
$SrcDir = Join-Path $ProjectRoot 'src'
$BuildDir = Join-Path $ProjectRoot 'build'

if(-not (Test-Path $BuildDir)) { New-Item -ItemType Directory -Path $BuildDir | Out-Null }

# Find all C sources
$Sources = Get-ChildItem -Path $SrcDir -Recurse -Filter *.c |
  Where-Object { $_.Name -notin @('commands.c','calibration.c') } |
  ForEach-Object { $_.FullName }
if(-not $Sources) { Write-Error 'No C sources found under src/'; exit 1 }

$Elf = Join-Path $BuildDir 'firmware.elf'
$Hex = Join-Path $BuildDir 'firmware.hex'

$Defines = "-DF_CPU=16000000UL"
$CFlags = @('-mmcu=atmega328p','-Os','-Wall','-Wextra','-Werror', $Defines, '-ffunction-sections','-fdata-sections')
$LFlags = @('-Wl,--gc-sections')
$Includes = @(
  "-I$SrcDir",
  "-I$SrcDir\hal",
  "-I$SrcDir\drivers",
  "-I$SrcDir\\app",
  "-I$SrcDir\\utils",
  "-I$SrcDir\platform"
)

function Find-Tool($name){
  $cmd = Get-Command $name -ErrorAction SilentlyContinue
  if($cmd){ return $cmd.Path }
  $candidates = @(
    "$env:LOCALAPPDATA\Arduino15\packages\arduino\tools\avr-gcc",
    "$env:ProgramFiles(x86)\Microchip\avr8-gnu-toolchain\bin",
    "$env:ProgramFiles\Microchip\avr8-gnu-toolchain\bin"
  )
  foreach($base in $candidates){
    if(Test-Path $base){
      $exe = Get-ChildItem -Path $base -Recurse -Filter "$name.exe" -ErrorAction SilentlyContinue | Select-Object -First 1
      if($exe){ return $exe.FullName }
    }
  }
  return $null
}

$gcc = Find-Tool 'avr-gcc'
$size = Find-Tool 'avr-size'
$objcopy = Find-Tool 'avr-objcopy'

if(-not $gcc){ Write-Error "avr-gcc not found. Add to PATH or install Arduino AVR Boards or Microchip avr8-gnu-toolchain."; exit 1 }
if(-not $size){ Write-Warning "avr-size not found; size report will be skipped." }
if(-not $objcopy){ Write-Error "avr-objcopy not found. Add to PATH or install toolchain."; exit 1 }

Write-Host "[build] avr-gcc: $gcc"
if($size){ Write-Host "[build] avr-size: $size" }
Write-Host "[build] avr-objcopy: $objcopy"
Write-Host "[build] Using avr-gcc to compile $(($Sources | Measure-Object).Count) files..."

# Compile and link in one step
& $gcc @CFlags @Includes @Sources @LFlags -o $Elf
if($LASTEXITCODE -ne 0){ Write-Error "avr-gcc failed ($LASTEXITCODE)"; exit $LASTEXITCODE }

# Create HEX
& $objcopy -O ihex -R .eeprom $Elf $Hex
if($LASTEXITCODE -ne 0){ Write-Error "avr-objcopy failed ($LASTEXITCODE)"; exit $LASTEXITCODE }

# Print size
if($size){ & $size -C --mcu=atmega328p $Elf }

Write-Host "[build] Output: $Elf`n[build] HEX: $Hex"
