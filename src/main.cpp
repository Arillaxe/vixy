#include <filesystem>
#include <iostream>
#include <app.hpp>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

static void setWorkingDirectoryToExecutable()
{
  std::wstring path(MAX_PATH, L'\0');

  while (true)
  {
    DWORD length = GetModuleFileNameW(nullptr, path.data(), static_cast<DWORD>(path.size()));

    if (length == 0)
    {
      return;
    }

    if (length < path.size())
    {
      path.resize(length);
      break;
    }

    path.resize(path.size() * 2);
  }

  std::error_code error;
  std::filesystem::current_path(std::filesystem::path(path).parent_path(), error);
}

int main()
{
  setWorkingDirectoryToExecutable();

  try
  {
    App app;
    app.run();
  }
  catch (const std::exception &e)
  {
    std::cerr << e.what() << std::endl;

    return EXIT_FAILURE;
  }
  catch (...)
  {
    std::cerr << "Unknown error" << std::endl;

    return EXIT_FAILURE;
  }

  return EXIT_SUCCESS;
}
