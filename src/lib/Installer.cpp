#include <FSHub/Installer.hpp>

#include <nlohmann/json.hpp>

#include <format>
#include <regex>

namespace FSHub {

std::optional<ReleaseAsset> SelectAsset(
  const nlohmann::json& release, const std::string& assetPattern) {
  if (!release.contains("assets")) {
    return std::nullopt;
  }

  auto pattern = assetPattern;
  auto flags = std::regex::ECMAScript;
  if (pattern.starts_with("(?i)")) {
    pattern = pattern.substr(4);
    flags |= std::regex::icase;
  }
  const std::regex regex {pattern, flags};

  for (const auto& asset: release.at("assets")) {
    const std::string name = asset.value("name", "");
    if (std::regex_match(name, regex)) {
      return ReleaseAsset {
        .name = name,
        .downloadUrl = asset.value("browser_download_url", ""),
      };
    }
  }
  return std::nullopt;
}

}  // namespace FSHub

#ifdef _WIN32

#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Storage.Streams.h>
#include <winrt/Windows.Web.Http.Headers.h>
#include <winrt/Windows.Web.Http.h>

#include <windows.h>

#include <shldisp.h>
#include <shlobj.h>

#include <wil/com.h>

#include <fstream>

namespace FSHub {

namespace {

winrt::Windows::Web::Http::HttpClient MakeHttpClient() {
  winrt::Windows::Web::Http::HttpClient client;
  client.DefaultRequestHeaders().UserAgent().TryParseAdd(L"FlightSimHub");
  return client;
}

}  // namespace

std::expected<nlohmann::json, std::string> FetchJson(const std::string& url) {
  try {
    const auto client = MakeHttpClient();
    const winrt::Windows::Foundation::Uri uri {winrt::to_hstring(url)};
    const auto response = client.GetAsync(uri).get();
    if (!response.IsSuccessStatusCode()) {
      return std::unexpected(std::format(
        "GitHub API returned {} for {}",
        static_cast<int>(response.StatusCode()),
        url));
    }
    const auto body
      = winrt::to_string(response.Content().ReadAsStringAsync().get());
    return nlohmann::json::parse(body);
  } catch (const winrt::hresult_error& e) {
    return std::unexpected(winrt::to_string(e.message()));
  } catch (const std::exception& e) {
    return std::unexpected(e.what());
  }
}

std::expected<nlohmann::json, std::string> FetchLatestReleaseJson(
  const std::string& repo) {
  auto latest = FetchJson(
    std::format("https://api.github.com/repos/{}/releases/latest", repo));
  if (latest) {
    return latest;
  }
  // Projects that only publish prereleases (e.g. AITrack) have no
  // "latest" release; fall back to the newest entry in the full list
  const auto releases = FetchJson(
    std::format("https://api.github.com/repos/{}/releases?per_page=10", repo));
  if (!releases) {
    return std::unexpected(latest.error());
  }
  for (const auto& release: *releases) {
    if (!release.value("draft", false)) {
      return release;
    }
  }
  return std::unexpected(std::format("no releases found for {}", repo));
}

std::expected<void, std::string> DownloadFile(
  const std::string& url,
  const std::filesystem::path& destFile,
  const ProgressCallback& onProgress) {
  try {
    const auto client = MakeHttpClient();
    const winrt::Windows::Foundation::Uri uri {winrt::to_hstring(url)};
    const auto response
      = client
          .GetAsync(
            uri,
            winrt::Windows::Web::Http::HttpCompletionOption::
              ResponseHeadersRead)
          .get();
    if (!response.IsSuccessStatusCode()) {
      return std::unexpected(
        std::format("download failed with HTTP {}",
        static_cast<int>(response.StatusCode())));
    }

    uint64_t total = 0;
    if (const auto length = response.Content().Headers().ContentLength()) {
      total = length.Value();
    }

    const auto input = response.Content().ReadAsInputStreamAsync().get();
    std::ofstream out(destFile, std::ios::binary | std::ios::trunc);
    if (!out.good()) {
      return std::unexpected("cannot open destination file");
    }

    winrt::Windows::Storage::Streams::Buffer buffer(1 << 20);
    uint64_t received = 0;
    while (true) {
      const auto chunk
        = input
            .ReadAsync(
              buffer,
              buffer.Capacity(),
              winrt::Windows::Storage::Streams::InputStreamOptions::Partial)
            .get();
      if (chunk.Length() == 0) {
        break;
      }
      out.write(
        reinterpret_cast<const char*>(chunk.data()), chunk.Length());
      received += chunk.Length();
      if (onProgress && total > 0) {
        onProgress(static_cast<double>(received) / total);
      }
    }
    out.flush();
    if (!out.good()) {
      return std::unexpected("failed writing the download (disk full?)");
    }
    return {};
  } catch (const winrt::hresult_error& e) {
    return std::unexpected(winrt::to_string(e.message()));
  } catch (const std::exception& e) {
    return std::unexpected(e.what());
  }
}

std::expected<void, std::string> RunInstallerAndWait(
  const std::filesystem::path& installer) {
  SHELLEXECUTEINFOW info {.cbSize = sizeof(info)};
  info.fMask = SEE_MASK_NOCLOSEPROCESS;
  // ShellExecuteEx rather than CreateProcess: handles .msi and UAC elevation
  info.lpVerb = L"open";
  const auto file = installer.wstring();
  info.lpFile = file.c_str();
  info.nShow = SW_SHOWNORMAL;
  if (!ShellExecuteExW(&info)) {
    return std::unexpected(
      std::format("failed to start installer (error {})", GetLastError()));
  }
  if (info.hProcess) {
    WaitForSingleObject(info.hProcess, INFINITE);
    CloseHandle(info.hProcess);
  }
  return {};
}

namespace {

// Files and bytes under a directory; used to detect when the shell's
// asynchronous CopyHere has settled
std::pair<uintmax_t, uintmax_t> SnapshotTree(
  const std::filesystem::path& dir) {
  uintmax_t files = 0, bytes = 0;
  std::error_code ec;
  for (auto it = std::filesystem::recursive_directory_iterator(dir, ec);
       it != std::filesystem::recursive_directory_iterator();
       it.increment(ec)) {
    if (ec) {
      break;
    }
    if (it->is_regular_file(ec)) {
      ++files;
      bytes += it->file_size(ec);
    }
  }
  return {files, bytes};
}

}  // namespace

std::expected<void, std::string> ExtractZip(
  const std::filesystem::path& zipFile, const std::filesystem::path& destDir) {
  // A fresh destination keeps old versions from merging with new ones and
  // keeps the completion check below meaningful
  std::error_code ec;
  std::filesystem::remove_all(destDir, ec);
  std::filesystem::create_directories(destDir);

  auto shell = wil::CoCreateInstance<IShellDispatch>(CLSID_Shell);

  wil::unique_variant source;
  source.vt = VT_BSTR;
  source.bstrVal = SysAllocString(zipFile.wstring().c_str());
  wil::unique_variant dest;
  dest.vt = VT_BSTR;
  dest.bstrVal = SysAllocString(destDir.wstring().c_str());

  wil::com_ptr<Folder> sourceFolder;
  if (FAILED(shell->NameSpace(source, &sourceFolder)) || !sourceFolder) {
    return std::unexpected("cannot open zip file");
  }
  wil::com_ptr<Folder> destFolder;
  if (FAILED(shell->NameSpace(dest, &destFolder)) || !destFolder) {
    return std::unexpected("cannot open destination folder");
  }

  wil::com_ptr<FolderItems> items;
  if (FAILED(sourceFolder->Items(&items)) || !items) {
    return std::unexpected("cannot enumerate zip contents");
  }

  wil::unique_variant itemsVariant;
  itemsVariant.vt = VT_DISPATCH;
  itemsVariant.pdispVal = items.get();
  items->AddRef();

  wil::unique_variant options;
  options.vt = VT_I4;
  // 4: no progress dialog, 16: yes to all, 512: no confirm dir,
  // 1024: no error UI
  options.lVal = 4 | 16 | 512 | 1024;

  // CopyHere is asynchronous in some shell versions; poll for completion
  if (FAILED(destFolder->CopyHere(itemsVariant, options))) {
    return std::unexpected("zip extraction failed");
  }

  long expected = 0;
  items->get_Count(&expected);

  // CopyHere gives no completion signal; wait until every top-level item
  // exists and the tree has stopped growing for a couple of seconds
  auto last = SnapshotTree(destDir);
  int stablePolls = 0;
  for (int attempt = 0; attempt < 1200; ++attempt) {
    Sleep(250);

    long topLevel = 0;
    for (auto it = std::filesystem::directory_iterator(destDir, ec);
         !ec && it != std::filesystem::directory_iterator();
         it.increment(ec)) {
      ++topLevel;
    }

    const auto now = SnapshotTree(destDir);
    if (topLevel >= expected && now.first > 0 && now == last) {
      if (++stablePolls >= 8) {
        return {};
      }
    } else {
      stablePolls = 0;
    }
    last = now;
  }
  return std::unexpected("zip extraction timed out");
}

std::expected<void, std::string> InstallApp(
  const AppDefinition& app,
  const std::filesystem::path& managedAppsDir,
  const ProgressCallback& onProgress) {
  // This runs on background threads of fire_and_forget coroutines where an
  // escaped exception is std::terminate; every failure must become an error
  try {
    if (app.source.type != SourceType::GitHub) {
      return std::unexpected("app has no automatic install source");
    }

    const auto release = FetchLatestReleaseJson(app.source.repo);
    if (!release) {
      return std::unexpected(release.error());
    }

    const auto asset = SelectAsset(*release, app.source.assetPattern);
    if (!asset) {
      return std::unexpected(
        std::format("no release asset matches '{}'", app.source.assetPattern));
    }

    const auto downloaded
      = std::filesystem::temp_directory_path() / asset->name;

    if (const auto result
        = DownloadFile(asset->downloadUrl, downloaded, onProgress);
        !result) {
      return result;
    }

    if (app.source.installKind == InstallKind::Portable) {
      return ExtractZip(downloaded, managedAppsDir / app.id);
    }
    return RunInstallerAndWait(downloaded);
  } catch (const winrt::hresult_error& e) {
    return std::unexpected(winrt::to_string(e.message()));
  } catch (const std::exception& e) {
    return std::unexpected(e.what());
  }
}

}  // namespace FSHub

#endif  // _WIN32
