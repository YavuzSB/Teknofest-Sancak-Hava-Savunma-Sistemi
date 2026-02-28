param(
  [int]$MaxBytes = 400000,
  [string]$OutDirName = "ai_share_plain_400kb",
  [string]$Prefix = "SOURCE",
  [switch]$IncludeModels
)

$ErrorActionPreference = "Stop"

# Use UTF-8 with BOM for maximum Windows/editor compatibility.
$Utf8Bom = New-Object System.Text.UTF8Encoding($true)

function Is-ExcludedPath([string]$fullPath, [string]$repoRoot, [string]$outDirNameLower) {
  $rel = $fullPath.Substring($repoRoot.Length).TrimStart('\','/')
  if ($rel -eq "") { return $true }

  $relNorm  = ($rel -replace "/","\\")
  $relLower = $relNorm.ToLowerInvariant()

  # Exclude common generated/vendor dirs anywhere in the path
  $excludedDirRegexes = @(
    '(^|\\)\.git(\\|$)',
    '(^|\\)build[^\\]*(\\|$)',
    '(^|\\)cmake-build[^\\]*(\\|$)',
    '(^|\\)_deps(\\|$)',
    '(^|\\)\.vs(\\|$)',
    '(^|\\)\.vscode(\\|$)',
    '(^|\\)node_modules(\\|$)',
    '(^|\\)dist[^\\]*(\\|$)',
    '(^|\\)out(\\|$)',
    '(^|\\)bin(\\|$)',
    '(^|\\)obj(\\|$)'
  )

  foreach ($rx in $excludedDirRegexes) {
    if ($relLower -match $rx) { return $true }
  }

  # Exclude any previous exports (ai_share, ai_share_2mb, ai_share_plain_*, ...)
  if ($relLower -match '(^|\\)ai_share[^\\]*(\\|$)') { return $true }

  # Exclude the current output directory by name (in case it's not ai_share*)
  if ($outDirNameLower -and ($relLower -match ('(^|\\)' + [regex]::Escape($outDirNameLower) + '(\\|$)'))) {
    return $true
  }

  # Exclude common binaries / large assets
  $ext = [IO.Path]::GetExtension($fullPath).ToLowerInvariant()
  $excludedExt = @(
    ".onnx",
    ".png", ".jpg", ".jpeg", ".bmp", ".gif", ".webp",
    ".mp4", ".avi", ".mov", ".mkv",
    ".zip", ".7z", ".rar",
    ".exe", ".dll", ".so", ".dylib",
    ".a", ".lib",
    ".obj", ".o",
    ".pdb",
    ".bin"
  )

  if (-not $IncludeModels -and $ext -eq ".onnx") { return $true }
  if ($excludedExt -contains $ext) { return $true }

  return $false
}

function Try-ReadTextFile([string]$path) {
  try {
    return [IO.File]::ReadAllText($path, [Text.Encoding]::UTF8)
  } catch {
    try {
      return Get-Content -LiteralPath $path -Raw
    } catch {
      return $null
    }
  }
}

$repoRoot = Split-Path -Parent $PSScriptRoot
$outDir = Join-Path $repoRoot $OutDirName
$outDirNameLower = $OutDirName.ToLowerInvariant()

New-Item -ItemType Directory -Force -Path $outDir | Out-Null

# Clean previous outputs for this prefix
Get-ChildItem -Path $outDir -File -Filter ("{0}_part*.txt" -f $Prefix) -ErrorAction SilentlyContinue | Remove-Item -Force -ErrorAction SilentlyContinue
Get-ChildItem -Path $outDir -File -Filter "INDEX.txt" -ErrorAction SilentlyContinue | Remove-Item -Force -ErrorAction SilentlyContinue

$indexPath = Join-Path $outDir "INDEX.txt"
$skipped  = New-Object System.Collections.Generic.List[string]
$included = New-Object System.Collections.Generic.List[string]

# Select source-like files
$allowedExt = @(
  ".cpp", ".hpp", ".h", ".c",
  ".py",
  ".md", ".txt",
  ".cmake", ".yml", ".yaml",
  ".ini", ".json",
  ".bat", ".ps1",
  ".sh"
)

$allFiles = Get-ChildItem -Path $repoRoot -Recurse -File | Where-Object {
  $ext = $_.Extension.ToLowerInvariant()
  ($allowedExt -contains $ext) -and (-not (Is-ExcludedPath -fullPath $_.FullName -repoRoot $repoRoot -outDirNameLower $outDirNameLower))
} | Sort-Object FullName

$part = 1
$currentPath = Join-Path $outDir ("{0}_part{1:00}.txt" -f $Prefix, $part)
$currentBytes = 0

$headerLines = @(
  "AI Source Export (PLAIN TEXT)",
  ("Repo root: {0}" -f (Split-Path -Leaf $repoRoot)),
  ("Generated: {0}" -f (Get-Date).ToString("yyyy-MM-dd HH:mm:ss")),
  ("MaxBytes per part: {0}" -f $MaxBytes),
  "",
  "NOTE: This export is fence-free plain text for tools that fail to parse Markdown code blocks.",
  "",
  "Upload order:",
  "1) INDEX.txt",
  ("2) {0}_part01.txt, {0}_part02.txt, ... (in order)" -f $Prefix),
  ""
)

$header = [string]::Join("`n", $headerLines)

[IO.File]::WriteAllText($currentPath, $header + "`n", $Utf8Bom)
$currentBytes = [Text.Encoding]::UTF8.GetByteCount($header + "`n")

foreach ($f in $allFiles) {
  $rel = $f.FullName.Substring($repoRoot.Length).TrimStart('\','/')
  $rel = $rel -replace "/","\\"

  $content = Try-ReadTextFile -path $f.FullName
  if ($null -eq $content) {
    $skipped.Add(("SKIP (read failed): {0}" -f $rel)) | Out-Null
    continue
  }

  # Guard against accidentally exporting huge text blobs (logs, dumps)
  $contentBytes = [Text.Encoding]::UTF8.GetByteCount($content)
  if ($contentBytes -gt 5000000) {
    $skipped.Add(("SKIP (too large text file): {0} ({1} bytes)" -f $rel, $contentBytes)) | Out-Null
    continue
  }

  $relSlash = $rel.Replace('\\','/')
  $blockLines = @(
    "================================================================================",
    ("FILE: {0}" -f $relSlash),
    "================================================================================",
    $content.TrimEnd(),
    "",
    ("END FILE: {0}" -f $relSlash),
    ""
  )

  $block = [string]::Join("`n", $blockLines)
  $blockBytes = [Text.Encoding]::UTF8.GetByteCount($block + "`n")

  if (($currentBytes + $blockBytes) -gt $MaxBytes) {
    $part++
    $currentPath = Join-Path $outDir ("{0}_part{1:00}.txt" -f $Prefix, $part)
    [IO.File]::WriteAllText($currentPath, $header + "`n", $Utf8Bom)
    $currentBytes = [Text.Encoding]::UTF8.GetByteCount($header + "`n")
  }

  [IO.File]::AppendAllText($currentPath, $block + "`n", $Utf8Bom)
  $currentBytes += $blockBytes
  $included.Add($relSlash) | Out-Null
}

$parts = Get-ChildItem -Path $outDir -Filter ("{0}_part*.txt" -f $Prefix) | Sort-Object Name

$index = New-Object System.Collections.Generic.List[string]
$index.Add("AI Export Index (PLAIN TEXT)") | Out-Null
$index.Add("") | Out-Null
$index.Add("Upload order (NotebookLM / similar tools):") | Out-Null
$index.Add("- First upload: INDEX.txt") | Out-Null
$index.Add("- Then upload: " + (($parts | ForEach-Object { $_.Name }) -join ", ")) | Out-Null
$index.Add("") | Out-Null
$index.Add(("OutDir: {0}" -f $OutDirName)) | Out-Null
$index.Add(("Included files: {0}" -f $included.Count)) | Out-Null
$index.Add(("Skipped files: {0}" -f $skipped.Count)) | Out-Null
$index.Add("") | Out-Null
$index.Add("Parts:") | Out-Null
foreach ($p in $parts) { $index.Add("- {0}" -f $p.Name) | Out-Null }
$index.Add("") | Out-Null
$index.Add("Included paths:") | Out-Null
foreach ($i in $included) { $index.Add("- {0}" -f $i) | Out-Null }

if ($skipped.Count -gt 0) {
  $index.Add("") | Out-Null
  $index.Add("Skipped (with reasons):") | Out-Null
  foreach ($s in $skipped) { $index.Add("- {0}" -f $s) | Out-Null }
}

[IO.File]::WriteAllText($indexPath, ([string]::Join("`n", $index)) + "`n", $Utf8Bom)

Write-Host "Export complete -> $outDir"
Write-Host ("Parts: {0}" -f $parts.Count)
Write-Host ("Included: {0}, Skipped: {1}" -f $included.Count, $skipped.Count)
