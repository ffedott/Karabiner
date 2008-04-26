#include <sys/time.h>
#include <sys/types.h>
#include <sys/sysctl.h>
#include <stdio.h>
#include <stdlib.h>

#include <iostream>
#include <fstream>

#include <CoreFoundation/CoreFoundation.h>

namespace {
  CFStringRef applicationID = CFSTR("org.pqrs.KeyRemap4MacBook");

  // ============================================================
  // SAVE & LOAD
  CFMutableDictionaryRef dict_sysctl = NULL;

  void
  save(const char *name)
  {
    if (! dict_sysctl) return;

    char entry[512];
    snprintf(entry, sizeof(entry), "keyremap4macbook.%s", name);

    int value;
    size_t len = sizeof(value);
    if (sysctlbyname(entry, &value, &len, NULL, 0) == -1) return;

    CFStringRef key = CFStringCreateWithCString(NULL, name, kCFStringEncodingUTF8);
    CFNumberRef val = CFNumberCreate(NULL, kCFNumberIntType, &value);
    CFDictionarySetValue(dict_sysctl, key, val);
  }

  void
  load(const char *name)
  {
    if (! dict_sysctl) return;
    CFStringRef key = CFStringCreateWithCString(NULL, name, kCFStringEncodingUTF8);

    CFNumberRef val = reinterpret_cast<CFNumberRef>(CFDictionaryGetValue(dict_sysctl, key));
    if (! val) return;

    int value;
    if (! CFNumberGetValue(val, kCFNumberIntType, &value)) return;

    char cmd[512];
    snprintf(cmd, sizeof(cmd), "/Library/org.pqrs/KeyRemap4MacBook/bin/KeyRemap4MacBook_sysctl_set %s %d", name, value);
    system(cmd);
  }

  void
  scanLines(const char *filename, void (*func)(const char *))
  {
    std::ifstream ifs(filename);
    if (! ifs) return;

    while (! ifs.eof()) {
      char line[512];

      ifs.getline(line, sizeof(line));

      const char *sysctl_begin = "<sysctl>";
      const char *sysctl_end = "</sysctl>";

      char *begin = strstr(line, "<sysctl>");
      if (! begin) continue;
      char *end = strstr(line, sysctl_end);
      if (! end) continue;

      begin += strlen(sysctl_begin);
      *end = '\0';

      func(begin);
    }
  }

  bool
  saveToFile(const char **targetFiles, CFStringRef identify)
  {
    dict_sysctl = CFDictionaryCreateMutable(NULL, 0, NULL, NULL);
    if (! dict_sysctl) return false;

    for (int i = 0; ; ++i) {
      const char *filename = targetFiles[i];
      if (! filename) break;
      scanLines(filename, save);
    }
    CFPreferencesSetAppValue(identify, dict_sysctl, applicationID);

    CFRelease(dict_sysctl); dict_sysctl = NULL;
    return true;
  }

  bool
  loadFromFile(const char **targetFiles, CFStringRef identify)
  {
    dict_sysctl = reinterpret_cast<CFMutableDictionaryRef>(const_cast<void *>(CFPreferencesCopyAppValue(identify, applicationID)));
    if (! dict_sysctl) return false;

    for (int i = 0; ; ++i) {
      const char *filename = targetFiles[i];
      if (! filename) break;
      scanLines(filename, load);
    }

    CFRelease(dict_sysctl); dict_sysctl = NULL;
    return true;
  }

  // ============================================================
  // ADD & DELETE & SELECT
  void
  setConfigList(CFArrayRef list)
  {
    CFPreferencesSetAppValue(CFSTR("configList"), list, applicationID);
  }

  CFArrayRef
  getConfigList(void)
  {
    CFArrayRef list = reinterpret_cast<CFArrayRef>(CFPreferencesCopyAppValue(CFSTR("configList"), applicationID));

    if (! list || CFArrayGetCount(list) == 0) {
      if (list) CFRelease(list);

      CFMutableDictionaryRef dict[1];
      dict[0] = CFDictionaryCreateMutable(NULL, 0, NULL, NULL);
      CFDictionarySetValue(dict[0], CFSTR("name"), CFSTR("Default"));
      CFDictionarySetValue(dict[0], CFSTR("identify"), CFSTR("config_default"));

      list = CFArrayCreate(NULL, const_cast<const void **>(reinterpret_cast<void **>(dict)), 1, NULL);
      setConfigList(list);
    }

    return list;
  }

  CFDictionaryRef
  getConfigDictionary(int index)
  {
    CFArrayRef list = getConfigList();
    if (! list) return NULL;

    if (index < 0) return NULL;
    if (index >= CFArrayGetCount(list)) return NULL;

    return reinterpret_cast<CFDictionaryRef>(CFArrayGetValueAtIndex(list, index));
  }

  CFStringRef
  getIdentify(int index)
  {
    CFDictionaryRef dict = getConfigDictionary(index);
    if (! dict) return NULL;

    return reinterpret_cast<CFStringRef>(CFDictionaryGetValue(dict, CFSTR("identify")));
  }

  bool
  appendConfig(void)
  {
    CFArrayRef list = getConfigList();
    if (! list) return false;

    struct timeval tm;
    gettimeofday(&tm, NULL);
    CFStringRef identify = CFStringCreateWithFormat(NULL, NULL, CFSTR("config_%d_%d"), tm.tv_sec, tm.tv_usec);

    CFMutableDictionaryRef dict;
    dict = CFDictionaryCreateMutable(NULL, 0, NULL, NULL);
    CFDictionarySetValue(dict, CFSTR("name"), CFSTR("NewItem"));
    CFDictionarySetValue(dict, CFSTR("identify"), identify);

    CFMutableArrayRef newlist = CFArrayCreateMutableCopy(NULL, 0, list);
    CFArrayAppendValue(newlist, dict);

    setConfigList(newlist);
    return true;
  }

  bool
  removeConfig(int index)
  {
    CFStringRef identify = getIdentify(index);
    if (! identify) return false;

    CFPreferencesSetAppValue(identify, NULL, applicationID);

    CFArrayRef list = getConfigList();
    if (! list) return false;

    CFMutableArrayRef newlist = CFArrayCreateMutableCopy(NULL, 0, list);
    CFArrayRemoveValueAtIndex(newlist, index);

    setConfigList(newlist);
    return true;
  }


  bool
  renameConfig(int index, const char *newname)
  {
    CFArrayRef list = getConfigList();
    if (! list) return false;

    CFDictionaryRef olddict = getConfigDictionary(index);
    if (! olddict) return false;

    CFMutableDictionaryRef newdict = CFDictionaryCreateMutableCopy(NULL, 0, olddict);
    CFDictionarySetValue(newdict, CFSTR("name"), CFStringCreateWithCString(NULL, newname, kCFStringEncodingUTF8));

    CFMutableArrayRef newlist = CFArrayCreateMutableCopy(NULL, 0, list);
    CFArraySetValueAtIndex(newlist, index, newdict);

    setConfigList(newlist);
    return true;
  }

  // ----------------------------------------
  bool
  setSelectedIndex(int index)
  {
    CFDictionaryRef dict = getConfigDictionary(index);
    if (! dict) return false;

    CFNumberRef val = CFNumberCreate(NULL, kCFNumberIntType, &index);
    CFPreferencesSetAppValue(CFSTR("selectedIndex"), val, applicationID);

    return true;
  }

  int
  getSelectedIndex(void)
  {
    Boolean isOK;
    CFIndex value = CFPreferencesGetAppIntegerValue(CFSTR("selectedIndex"), applicationID, &isOK);
    if (! isOK || value < 0) {
      value = 0;
      setSelectedIndex(value);
    }
    return value;
  }
}


int
main(int argc, char **argv)
{
  if (argc == 1) {
    fprintf(stderr, "Usage: %s (save|load|add|delete|name|select) [params]\n", argv[0]);
    return 1;
  }

  bool isSuccess = false;

  if (strcmp(argv[1], "select") == 0) {
    if (argc < 3) {
      fprintf(stderr, "Usage: %s select index\n", argv[0]);
      goto finish;
    }
    int index = atoi(argv[2]);
    isSuccess = setSelectedIndex(index);

  } else if (strcmp(argv[1], "add") == 0) {
    isSuccess = appendConfig();

  } else if (strcmp(argv[1], "delete") == 0) {
    if (argc < 3) {
      fprintf(stderr, "Usage: %s delete index\n", argv[0]);
      goto finish;
    }
    int index = atoi(argv[2]);
    isSuccess = removeConfig(index);

  } else if (strcmp(argv[1], "name") == 0) {
    // sysctl_ctl name "index" "newname"
    if (argc < 4) {
      fprintf(stderr, "Usage: %s name index newname\n", argv[0]);
      goto finish;
    }
    int index = atoi(argv[2]);
    isSuccess = renameConfig(index, argv[3]);

  } else if ((strcmp(argv[1], "save") == 0) || (strcmp(argv[1], "load") == 0)) {
    int value = getSelectedIndex();
    CFStringRef identify = getIdentify(value);

    const char *targetFiles[] = {
      "/Library/org.pqrs/KeyRemap4MacBook/prefpane/checkbox.xml",
      "/Library/org.pqrs/KeyRemap4MacBook/prefpane/number.xml",
      NULL,
    };
    if (strcmp(argv[1], "save") == 0) {
      isSuccess = saveToFile(targetFiles, identify);
    }
    if (strcmp(argv[1], "load") == 0) {
      system("/Library/org.pqrs/KeyRemap4MacBook/bin/KeyRemap4MacBook_sysctl_reset");
      system("/Library/org.pqrs/KeyRemap4MacBook/bin/KeyRemap4MacBook_sysctl_set initialized 1");
      isSuccess = loadFromFile(targetFiles, identify);
    }
  }

  CFPreferencesAppSynchronize(applicationID);

finish:
  if (isSuccess) {
    fprintf(stderr, "[DONE]\n");
  } else {
    fprintf(stderr, "[ERROR]\n");
  }

  return 0;
}
