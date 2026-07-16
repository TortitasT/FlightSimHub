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

std::expected<nlohmann::json, std::string> FetchLatestReleaseJson(
  const std::string& repo) {
  try {
    const auto client = MakeHttpClient();
    const winrt::Windows::Foundation::Uri uri {winrt::to_hstring(
      std::format("https://api.github.com/repos/{}/releases/latest", repo))};
    const auto response = client.GetAsync(uri).get();
    if (!response.IsSuccessStatusCode()) {
      return std::unexpected(std::format(
        "GitHub API returned {} for {}",
        static_cast<int>(response.StatusCode()),
        repo));
    }
    const auto body
      = winrt::to_string(response.Content().ReadAsStringAsync().get());
    return nlohmann::json::parse(body);
  } catch (const std::exception& e) {
    return std::unexpected(e.what());
  } catch (const winrt::hresult_error& e) {
    return std::unexpected(winrt::to_string(e.message()));
  }
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

std::expected<void, std::string> ExtractZip(
  const std::filesystem::path& zipFile, const std::filesystem::path& destDir) {
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
  for (int attempt = 0; attempt < 300; ++attempt) {
    wil::com_ptr<FolderItems> extracted;
    if (SUCCEEDED(destFolder->Items(&extracted)) && extracted) {
      long count = 0;
      extracted->get_Count(&count);
      if (count >= expected) {
        return {};
      }
    }
    Sleep(100);
  }
  return std::unexpected("zip extraction timed out");
}

std::expected<void, std::string> InstallApp(
  const AppDefinition& app,
  const std::filesystem::path& managedAppsDir,
  const ProgressCallback& onProgress) {
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

  wchar_t tempDir[MAX_PATH] {};
  GetTempPathW(MAX_PATH, tempDir);
  const auto downloaded = std::filesystem::path(tempDir) / asset->name;

  if (const auto result
      = DownloadFile(asset->downloadUrl, downloaded, onProgress);
      !result) {
    return result;
  }

  if (app.source.installKind == InstallKind::Portable) {
    return ExtractZip(downloaded, managedAppsDir / app.id);
  }
  return RunInstallerAndWait(downloaded);
}

}  // namespace FSHub

#endif  // _WIN32
