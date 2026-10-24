#requires -version 5.1
<#
G30 bench UART transport, no SHU or ST-Link.
DRY RUN is the default. The live modes require a compatible, electrically
correct half-duplex/open-drain UART adapter. Never attach a push-pull TX
directly to the stock single-wire UART. COM port must not be owned by RemoteLab.
The script never sends flash, erase, reset, E5 or IAP commands.
#>
[CmdletBinding()]
param(
    [ValidateSet('DryRun','Listen','Identify','Snapshot','Neutral','Pulse')][string]$Mode = 'DryRun',
    [string]$Port,
    [ValidateRange(4,100)][int]$Throttle = 35,
    [ValidateRange(1,20)][int]$Seconds = 3,
    [switch]$WheelFreeConfirmed,
    [switch]$CurrentLimitedPSUConfirmed,
    [switch]$FirmwareIdentityConfirmed
)
$ErrorActionPreference = 'Stop'
function Frame([byte]$gas, [byte]$brake) {
    # 5A A5 LEN=3 SRC=0x21 DST=0x20 CMD=0x65 ARG=0 PAYLOAD={0,gas,brake}
    [byte[]]$b = @(0x5a,0xa5,3,0x21,0x20,0x65,0,0,$gas,$brake,0,0)
    [int]$sum=0
    for($i=2; $i -lt ($b.Length-2); $i++){ $sum=($sum+[int]$b[$i]) -band 0xffff }
    [int]$check=(0xffff -bxor $sum)
    $b[$b.Length-2]=[byte]($check -band 0xff)
    $b[$b.Length-1]=[byte](($check -shr 8) -band 0xff)
    return ,$b
}
function Hex([byte[]]$b) { return ([BitConverter]::ToString($b)) }
function ReadFrame([byte]$arg) {
    # Read-only native app query: SRC=0x3D -> ESC=0x20, cmd=0x01
    [byte[]]$b = @(0x5a,0xa5,0,0x3d,0x20,1,$arg,0,0)
    [int]$sum=0
    for($i=2;$i -lt ($b.Length-2);$i++){ $sum=($sum+[int]$b[$i]) -band 0xffff }
    [int]$chk=(0xffff -bxor $sum)
    $b[$b.Length-2]=[byte]($chk -band 0xff)
    $b[$b.Length-1]=[byte](($chk -shr 8) -band 0xff)
    return ,$b
}

[byte[]]$identity = @(0x5a,0xa5,1,0x3d,0x20,1,0x10,0x0e,0x82,0xff) # read-only identity query
$neutral = [byte[]](Frame 0 0)
[byte[]]$snapshotArgs = @(0x10,0xD0,0xD1,0xD2,0xD3,0xD4,0xD5,0xD7,0xD9,0xE8,0xEB,0xFC,0xFD,0xFE,0xFF)
$pulse = [byte[]](Frame ([byte]$Throttle) 0)
Write-Host "Read-only identity query: $(Hex $identity)"
Write-Host "Neutral dashboard 0x65: $(Hex $neutral)"
Write-Host "Test throttle dashboard 0x65: $(Hex $pulse)"
if($Mode -eq 'DryRun') {
    Write-Host 'Dry-run only: no serial port opened, nothing transmitted.'
    exit 0
}
if([string]::IsNullOrWhiteSpace($Port)){throw 'Specify -Port COMx for a live serial session.'}
if($Mode -eq 'Pulse' -and (-not $WheelFreeConfirmed -or -not $CurrentLimitedPSUConfirmed -or -not $FirmwareIdentityConfirmed)){
    throw 'Pulse requires -WheelFreeConfirmed -CurrentLimitedPSUConfirmed -FirmwareIdentityConfirmed. Never use on the vehicle with its wheel on the ground.'
}
$stamp = [DateTime]::UtcNow.ToString('yyyyMMdd_HHmmss')
$log = Join-Path (Get-Location) "G30_BENCH_$stamp.log"
$s = New-Object System.IO.Ports.SerialPort($Port,115200,[System.IO.Ports.Parity]::None,8,[System.IO.Ports.StopBits]::One)
$s.ReadTimeout=35; $s.WriteTimeout=250
$s.DtrEnable=$false; $s.RtsEnable=$false
$opened=$false
try{
    $s.Open();$opened=$true
    Add-Content -Path $log -Value "# UTC=$( [DateTime]::UtcNow.ToString("o") ) MODE=$Mode PORT=$Port BAUD=115200" -Encoding UTF8
    $start=[DateTime]::UtcNow
    $end=$start.AddSeconds($Seconds)
    [byte[]]$buffer=New-Object byte[] 4096
    $identitySent=$false
    $snapshotIx=0
    $snapshotLastMs=-1000
    while([DateTime]::UtcNow -lt $end){
        $elapsed=([DateTime]::UtcNow-$start).TotalMilliseconds
        if($Mode -eq 'Identify' -and -not $identitySent){
            $s.Write($identity,0,$identity.Length)
            $identitySent=$true
            Add-Content -Path $log -Value ("$(Get-Date -Format o) TX READ_ID " + (Hex $identity)) -Encoding UTF8
        }
        if($Mode -eq 'Snapshot' -and $snapshotIx -lt $snapshotArgs.Length -and ($elapsed-$snapshotLastMs) -ge 100){
            [byte[]]$query=ReadFrame $snapshotArgs[$snapshotIx]
            $s.Write($query,0,$query.Length)
            Add-Content -Path $log -Value ("$(Get-Date -Format o) TX READ_" + $snapshotArgs[$snapshotIx].ToString("X2") + " " + (Hex $query)) -Encoding UTF8
            $snapshotLastMs=$elapsed
            $snapshotIx++
        }
        if($Mode -eq 'Neutral' -or $Mode -eq 'Pulse'){
            # Pulse starts and finishes with fresh neutral 0x65 frames.
            if($Mode -eq 'Pulse' -and $elapsed -ge 800 -and $elapsed -le (($Seconds*1000)-350)){
                [byte[]]$frame=$pulse
            } else { [byte[]]$frame=$neutral }
            $s.Write($frame,0,$frame.Length)
            Add-Content -Path $log -Value ("$(Get-Date -Format o) TX " + (Hex $frame)) -Encoding UTF8
        }
        [int]$n=$s.BytesToRead
        if($n -gt 0){
            $n=[Math]::Min($n,$buffer.Length)
            $read=$s.Read($buffer,0,$n)
            if($read -gt 0){
                [byte[]]$bytes=$buffer[0..($read-1)]
                Add-Content -Path $log -Value ("$(Get-Date -Format o) RX " + (Hex $bytes)) -Encoding UTF8
            }
        }
        Start-Sleep -Milliseconds 50
    }
    if($Mode -eq 'Snapshot' -and $snapshotIx -ne $snapshotArgs.Length){
        Write-Warning "Only $snapshotIx/$($snapshotArgs.Length) read-only diagnostic requests were sent. Use -Seconds 4 or longer for a complete snapshot."
    }
    if($Mode -eq 'Pulse'){
        for($i=0;$i -lt 10;$i++){
            $s.Write($neutral,0,$neutral.Length)
            Start-Sleep -Milliseconds 50
        }
    }
} finally {
    if($opened) {
        try { if($Mode -eq 'Pulse'){$s.Write($neutral,0,$neutral.Length)} } catch {}
        $s.Close()
    }
    $s.Dispose()
}
Write-Host "One-pass capture: $log"
Get-FileHash -Algorithm SHA256 -Path $log | Format-List Hash,Path
Write-Host "Decode saved RX frames without another hardware session:"
Write-Host "  python bench/G30_Decode_Log.py --log `"$log`""
