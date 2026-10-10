# DeltaESC G30D Gen1: passive BEMF feasibility audit via READ-ONLY SWD.
# Does NOT measure back-EMF, does NOT program/erase/reset/hold CPU.
# On running firmware (SHFW or DRV), inspect existing ADC sequencer and GPIO
# configuration so unproven PB1 or PA6/PA7 are NEVER guessed as phase sensors.
[CmdletBinding()]
param(
    [string]$Cli = "STM32_Programmer_CLI.exe",
    [string]$OutputFolder = "."
)
Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"
$regs = [ordered]@{
    RCC_APB2ENR = "0x40021018"
    GPIOA_CRL = "0x40010800"
    GPIOA_CRH = "0x40010804"
    GPIOB_CRL = "0x40010C00"
    GPIOB_CRH = "0x40010C04"
    GPIOC_CRH = "0x40011004"
    TIM1_CCER = "0x40012C20"
    TIM1_BDTR = "0x40012C44"
    ADC1_CR1 = "0x40012404"
    ADC1_CR2 = "0x40012408"
    ADC1_SMPR1 = "0x4001240C"
    ADC1_SMPR2 = "0x40012410"
    ADC1_SQR1 = "0x4001242C"
    ADC1_SQR2 = "0x40012430"
    ADC1_SQR3 = "0x40012434"
    ADC1_JSQR = "0x40012438"
    ADC2_CR1 = "0x40012804"
    ADC2_CR2 = "0x40012808"
    ADC2_SMPR1 = "0x4001280C"
    ADC2_SMPR2 = "0x40012810"
    ADC2_SQR1 = "0x4001282C"
    ADC2_SQR2 = "0x40012830"
    ADC2_SQR3 = "0x40012834"
    ADC2_JSQR = "0x40012838"
}
if (!(Test-Path -LiteralPath $OutputFolder -PathType Container)) {
    New-Item -ItemType Directory -Path $OutputFolder -Force | Out-Null
}
$path = Join-Path $OutputFolder ("G30D_BEMF_AdcMap_" + (Get-Date -Format "yyyyMMdd_HHmmss") + ".txt")
"### READ-ONLY ADC/GPIO/TIM MAP - no data registers, no motor commands" | Set-Content -LiteralPath $path
"### Firmware at capture time is authoritative; stock and SHFW may differ" | Add-Content -LiteralPath $path
foreach ($kv in $regs.GetEnumerator()) {
    ("### " + $kv.Key + " " + $kv.Value) | Add-Content -LiteralPath $path
    try {
        $reply = (& $Cli -c port=SWD mode=HOTPLUG freq=1000 -r32 $kv.Value 0x4 2>&1 | Out-String)
        $reply | Add-Content -LiteralPath $path
        if ($LASTEXITCODE -ne 0) {
            "### FAILED: no more reads attempted" | Add-Content -LiteralPath $path
            Write-Warning "SWD read failed. No recovery or write attempted."
            break
        }
    } catch {
        ("### READ ERROR: " + $_.Exception.Message) | Add-Content -LiteralPath $path
        Write-Warning "SWD read error. No changes attempted."
        break
    }
    Write-Host ("READ " + $kv.Key)
}
Write-Host ("Saved raw audit: " + $path)
Write-Host "The audit does NOT prove BEMF sensing or safe motor power."
