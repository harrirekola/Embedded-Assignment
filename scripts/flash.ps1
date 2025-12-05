param(
	[string]$Port = "COM4",
	[int]$Baud = 115200, # Set to 0 to auto-try 115200 then 57600
	[string]$Programmer = "arduino",
	[string]$Mcu = "m328p"
)

$ErrorActionPreference = 'Stop'

function Find-Tool($name){
	$cmd = Get-Command $name -ErrorAction SilentlyContinue
	if($cmd){ return $cmd.Path }
	$candidates = @(
		"$env:LOCALAPPDATA\Arduino15\packages\arduino\tools\avrdude",
		"$env:ProgramFiles(x86)\Arduino\hardware\tools\avr\bin",
		"$env:ProgramFiles\Arduino\hardware\tools\avr\bin"
	)
	foreach($base in $candidates){
		if(Test-Path $base){
			$exe = Get-ChildItem -Path $base -Recurse -Filter "$name.exe" -ErrorAction SilentlyContinue | Select-Object -First 1
			if($exe){ return $exe.FullName }
		}
	}
	return $null
}

$ProjectRoot = Split-Path -Parent $MyInvocation.MyCommand.Path | Split-Path -Parent
$BuildDir = Join-Path $ProjectRoot 'build'
$Hex = Join-Path $BuildDir 'firmware.hex'
${HexRel} = 'build/firmware.hex'

if(-not (Test-Path $Hex)){
	Write-Error "HEX not found at '$Hex'. Build first (scripts/build.ps1)."
	exit 1
}

$avrdude = Find-Tool 'avrdude'
if(-not $avrdude){
	Write-Error "avrdude not found. Install Arduino AVR Boards or Microchip avrdude and add to PATH."
	exit 1
}

function Find-AvrdudeConf($exe){
	$dir = Split-Path -Parent $exe
	$bases = @(
		(Split-Path -Parent $dir),              # .../tools/avrdude/<ver>
		(Split-Path -Parent (Split-Path -Parent $dir))  # .../tools/avrdude
	) | Where-Object { $_ -ne $null }
	foreach($b in $bases){
		$cand = Join-Path $b 'etc\avrdude.conf'
		if(Test-Path $cand){ return $cand }
	}
	# Also try alongside common Arduino hardware tools layout: .../avr/etc/avrdude.conf
	$avrEtc = Join-Path (Split-Path -Parent $dir) 'etc\avrdude.conf'
	if(Test-Path $avrEtc){ return $avrEtc }
	return $null
}

$avrdudeConf = Find-AvrdudeConf -exe $avrdude
if(-not $avrdudeConf){
	Write-Warning "Could not locate avrdude.conf automatically; relying on avrdude defaults."
}

Write-Host "[flash] avrdude: $avrdude"
Write-Host "[flash] Port=$Port Programmer=$Programmer MCU=$Mcu"
Write-Host "[flash] Image=$Hex"

function Invoke-Flash([int]$rate){
	Write-Host "[flash] Trying baud $rate ..."
	$args = @()
	if($avrdudeConf){ $args += @('-C', $avrdudeConf) }
	$args += @('-p', $Mcu, '-c', $Programmer, '-P', $Port, '-b', $rate.ToString(), '-D', '-U', "flash:w:${HexRel}:i")
	Push-Location $ProjectRoot
	& $avrdude @args
	Pop-Location
	return $LASTEXITCODE
}

if($Baud -eq 0){
	$exit = Invoke-Flash -rate 115200
	if($exit -ne 0){
		Write-Warning "[flash] 115200 failed ($exit); falling back to 57600"
		$exit = Invoke-Flash -rate 57600
	}
	if($exit -ne 0){ Write-Error "avrdude failed ($exit)"; exit $exit }
} else {
	$exit = Invoke-Flash -rate $Baud
	if($exit -ne 0){ Write-Error "avrdude failed ($exit)"; exit $exit }
}

Write-Host "[flash] Done."
