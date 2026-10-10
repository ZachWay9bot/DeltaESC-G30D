# Ninebot G30D STM32F103 / SHFW or DRV: SWD READ ONLY.
# Installed firmware currently SHFW; captured registers reflect SHFW runtime, NOT stock DRV126.
# Hardware not validated for motor output.
# NO erase, write, flash, RDP change, option bytes, reset or run command.
[CmdletBinding()]
param(
    [string]$Cli = "STM32_Programmer_CLI.exe",
    [ValidateSet("Idle","Button")][string]$Phase = "Idle",
    [string]$OutputFolder = "."
)
Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"
$registers = [ordered]@{
    "RCC_APB2ENR"="0x40021018"
    "GPIOA_CRH"="0x40010804"
    "GPIOA_IDR"="0x40010808"
    "GPIOA_ODR"="0x4001080C"
    "GPIOB_CRL"="0x40010C00"
    "GPIOB_CRH"="0x40010C04"
    "GPIOB_IDR"="0x40010C08"
    "GPIOC_CRH"="0x40011004"
    "GPIOC_IDR"="0x40011008"
    "TIM1_CR1"="0x40012C00"
    "TIM1_CR2"="0x40012C04"
    "TIM1_CCMR1"="0x40012C18"
    "TIM1_CCMR2"="0x40012C1C"
    "TIM1_CCER"="0x40012C20"
    "TIM1_ARR"="0x40012C2C"
    "TIM1_CCR4"="0x40012C40"
    "TIM1_BDTR"="0x40012C44"
    "ADC1_CR1"="0x40012404"
    "ADC1_CR2"="0x40012408"
    "ADC1_JSQR"="0x40012438"
    "ADC2_CR2"="0x40012808"
    "ADC2_JSQR"="0x40012838"
}
if ($Phase -eq "Button") {
    # Keep button press short, NEVER 6 seconds.
    $registers=[ordered]@{"GPIOC_IDR"="0x40011008"}
}
if (!(Test-Path -LiteralPath $OutputFolder -PathType Container)) {
    New-Item -ItemType Directory -Path $OutputFolder -Force | Out-Null
}
$filename="DeltaESC_SHFW_SWD_"+$Phase+"_"+(Get-Date -Format "yyyyMMdd_HHmmss")+".txt"
$path=Join-Path $OutputFolder $filename
("SHFW-installed runtime / "+$Phase+" / no flash commands; NOT original DRV126") | Set-Content -LiteralPath $path
foreach ($item in $registers.GetEnumerator()) {
    ("### "+$item.Key+" "+$item.Value) | Add-Content -LiteralPath $path
    # ST UM2237: -r32 <address> <byte_count> is READ ONLY.
    try {
        $reply= (& $Cli -c port=SWD mode=HOTPLUG freq=1000 -r32 $item.Value 0x4 2>&1 | Out-String)
        $reply | Add-Content -LiteralPath $path
        if ($LASTEXITCODE -ne 0) {
            Write-Warning "Read failed; exiting without changing the MCU."
            break
        }
    } catch {
        $_.Exception.Message | Add-Content -LiteralPath $path
        Write-Warning "Read error; exiting without changing the MCU."
        break
    }
    Write-Host ("READ "+$item.Key)
}
Write-Host ("Capture saved: "+$path)
