using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Text;
using System.Text.RegularExpressions;



class CustomParser
{
    static readonly Regex ClassRegex = new Regex(
        @"UCLASS\s*\([^)]*\)\s*class\s+(?:[A-Z0-9_]+_API\s+)?(?<name>\w+)\s*:\s*public\s+(?<parent>\w+)",
        RegexOptions.Singleline);

    static readonly Regex PropertyRegex = new Regex(
        @"UPROPERTY\s*\((?<options>[^)]*)\)\s+(?<type>[A-Za-z_]\w*(?:::\w+)*(?:\s*<[^;{}()]+>)?\s*[*&]?)\s+(?<name>\w+)\s*(?:=\s*[^;]*|\{[^;]*\})?\s*;",
        RegexOptions.Singleline);

    static readonly Regex StructRegex = new Regex(
        @"USTRUCT\s*\([^)]*\)\s*struct\s+(?:[A-Z0-9_]+_API\s+)?(?<name>\w+)(?:\s*:\s*(?:public|protected|private)?\s+(?<parent>\w+))?",
        RegexOptions.Singleline);

    static readonly Regex EnumRegex = new Regex(
        @"UENUM\s*\([^)]*\)\s*enum\s+class\s+(?:[A-Z0-9_]+_API\s+)?(?<name>\w+)",
        RegexOptions.Singleline);

    #region Helper Methods

    static string GetPropertyInstantiationCode(string type, string varName)
    {
        type = type.Trim();

        // 1. 기본 숫자/불리언/문자열 타입 매핑
        if (type == "bool") return $"        FBoolProperty* {varName} = new FBoolProperty();";
        if (type == "int8") return $"        FInt8Property* {varName} = new FInt8Property();";
        if (type == "int16") return $"        FInt16Property* {varName} = new FInt16Property();";
        if (type == "int32" || type == "int") return $"        FIntProperty* {varName} = new FIntProperty();";
        if (type == "int64") return $"        FInt64Property* {varName} = new FInt64Property();";
        if (type == "uint8" || type == "byte") return $"        FByteProperty* {varName} = new FByteProperty();";
        if (type == "uint16") return $"        FUInt16Property* {varName} = new FUInt16Property();";
        if (type == "uint32") return $"        FUInt32Property* {varName} = new FUInt32Property();";
        if (type == "uint64") return $"        FUInt64Property* {varName} = new FUInt64Property();";
        if (type == "float") return $"        FFloatProperty* {varName} = new FFloatProperty();";
        if (type == "double") return $"        FDoubleProperty* {varName} = new FDoubleProperty();";
        if (type == "FName") return $"        FNameProperty* {varName} = new FNameProperty();";
        if (type == "FString") return $"        FStrProperty* {varName} = new FStrProperty();";
        if (type == "FText") return $"        FTextProperty* {varName} = new FTextProperty();";

        // 2. 배열 (TArray) - 재귀적으로 내부 프로퍼티까지 생성!
        Match arrayMatch = Regex.Match(type, @"^TArray\s*<\s*(.+)\s*>$");
        if (arrayMatch.Success)
        {
            string innerType = arrayMatch.Groups[1].Value.Trim();
            StringBuilder sb = new StringBuilder();
            sb.AppendLine($"        FArrayProperty* {varName} = new FArrayProperty();");
            sb.AppendLine($"        {varName}->ArrayOps = TScriptArrayOps<{innerType}>::Make();");

            string innerVar = varName + "_Inner";
            sb.AppendLine(GetPropertyInstantiationCode(innerType, innerVar));
            sb.AppendLine($"        {innerVar}->CPPType = \"{EscapeForCppString(innerType)}\";");
            sb.AppendLine($"        {innerVar}->ElementSize = sizeof({innerType});");
            sb.AppendLine($"        {varName}->Inner = {innerVar};");
            return sb.ToString();
        }

        // 3. 컨테이너 (TMap, TSet)
        if (type.StartsWith("TMap<")) return $"        FMapProperty* {varName} = new FMapProperty();";
        if (type.StartsWith("TSet<")) return $"        FSetProperty* {varName} = new FSetProperty();";

        // 4. 포인터 및 특수 포인터 (UObject 기반)
        if (type.EndsWith("*")) return $"        FObjectProperty* {varName} = new FObjectProperty();";
        if (type.StartsWith("FSoftObjectPtr") || type.StartsWith("TSoftObjectPtr<")) return $"        FSoftObjectProperty* {varName} = new FSoftObjectProperty();";
        if (type.StartsWith("FSoftClassPtr") || type.StartsWith("TSoftClassPtr<")) return $"        FSoftClassProperty* {varName} = new FSoftClassProperty();";
        if (type.StartsWith("FWeakObjectPtr") || type.StartsWith("TWeakObjectPtr<")) return $"        FWeakObjectProperty* {varName} = new FWeakObjectProperty();";
        if (type.StartsWith("FLazyObjectPtr") || type.StartsWith("TLazyObjectPtr<")) return $"        FLazyObjectProperty* {varName} = new FLazyObjectProperty();";
        if (type.StartsWith("FScriptInterface")) return $"        FInterfaceProperty* {varName} = new FInterfaceProperty();";

        // 5. Enum (언리얼 네이밍 규칙인 E로 시작한다고 가정)
        if (type.StartsWith("E")) return $"        FEnumProperty* {varName} = new FEnumProperty();";

        // 6. 그 외의 모든 것은 구조체(Struct)로 간주
        return $"        FStructProperty* {varName} = new FStructProperty();";
    }

    static string GeneratePropertyCode(string cppType, string propName, string ownerName, ParsedPropertyOptions options)
    {
        StringBuilder sb = new StringBuilder();
        sb.AppendLine("    {");

        // 타입에 맞는 C++ 인스턴스 생성 코드 가져오기 (예: FIntProperty* prop = new FIntProperty();)
        sb.AppendLine(GetPropertyInstantiationCode(cppType, "prop"));

        // 공통 속성(부모 FProperty) 데이터 채워넣기
        sb.AppendLine($"        prop->Name = FName(\"{EscapeForCppString(propName)}\");");
        sb.AppendLine($"        prop->CPPType = \"{EscapeForCppString(cppType)}\";");
        sb.AppendLine($"        prop->Offset = offsetof({ownerName}, {propName});");
        sb.AppendLine($"        prop->ElementSize = sizeof({cppType});");
        sb.AppendLine($"        prop->Flags = {options.FlagsExpression};");
        sb.AppendLine($"        prop->Category = \"{EscapeForCppString(options.Category)}\";");

        string disp = string.IsNullOrEmpty(options.DisplayName) ? propName : options.DisplayName;
        sb.AppendLine($"        prop->DisplayName = \"{EscapeForCppString(disp)}\";");

        // FClassInfo / FStructInfo의 ReflectedProperties 배열에 넣기
        sb.AppendLine("        info.ReflectedProperties.push_back(prop);");
        sb.AppendLine("    }");
        return sb.ToString();
    }

    struct ParsedPropertyOptions
    {
        public string FlagsExpression;
        public string Category;
        public string DisplayName;
    }

    static List<string> SplitPropertyOptions(string options)
    {
        List<string> result = new List<string>();
        int start = 0;
        int parenDepth = 0;
        bool inString = false;
        bool escape = false;

        for (int i = 0; i <= options.Length; i++)
        {
            bool atEnd = i == options.Length;
            char c = atEnd ? ',' : options[i];

            if (!atEnd)
            {
                if (inString)
                {
                    if (escape)
                        escape = false;
                    else if (c == '\\')
                        escape = true;
                    else if (c == '"')
                        inString = false;
                }
                else
                {
                    if (c == '"')
                        inString = true;
                    else if (c == '(')
                        parenDepth++;
                    else if (c == ')')
                        parenDepth--;
                }
            }

            if ((atEnd || c == ',') && !inString && parenDepth == 0)
            {
                string item = options.Substring(start, i - start).Trim();
                start = i + 1;

                if (item.Length > 0)
                    result.Add(item);
            }
        }

        return result;
    }

    static ParsedPropertyOptions ParsePropertyOptions(string options)
    {
        List<string> flags = new List<string>();
        string category = "Default";
        string displayName = "";

        foreach (string rawOption in SplitPropertyOptions(options))
        {
            string option = rawOption.Trim();

            if (option.Equals("EditAnywhere", StringComparison.Ordinal))
            {
                flags.Add("PF_EditAnywhere");
            }
            else if (option.Equals("VisibleAnywhere", StringComparison.Ordinal))
            {
                flags.Add("PF_VisibleAnywhere");
            }
            else if (option.Equals("HideInEditor", StringComparison.Ordinal))
            {
                flags.Add("PF_HideInEditor");
            }
            else if (option.Equals("Transient", StringComparison.Ordinal))
            {
                flags.Add("PF_Transient");
            }
            else if (option.Equals("SaveGame", StringComparison.Ordinal))
            {
                flags.Add("PF_SaveGame");
            }
            else
            {
                Match categoryMatch = Regex.Match(option, @"^Category\s*=\s*""(?<category>[^""]*)""$");
                if (categoryMatch.Success)
                {
                    category = categoryMatch.Groups["category"].Value;
                    continue;
                }

                Match displayNameMatch = Regex.Match(option, @"^(?:DisplayName|Display)\s*=\s*""(?<displayName>[^""]*)""$");
                if (displayNameMatch.Success)
                    displayName = displayNameMatch.Groups["displayName"].Value;
            }
        }

        string flagsExpression = flags.Count > 0 ? string.Join(" | ", flags) : "PF_None";

        return new ParsedPropertyOptions
        {
            FlagsExpression = flagsExpression,
            Category = category,
            DisplayName = displayName
        };
    }

    static List<string> ParseEnumValueNames(string enumBody)
    {
        List<string> values = new List<string>();

        int start = 0;
        int angleDepth = 0;
        int parenDepth = 0;
        int braceDepth = 0;

        for (int i = 0; i <= enumBody.Length; i++)
        {
            bool atEnd = i == enumBody.Length;
            char c = atEnd ? ',' : enumBody[i];

            if (!atEnd)
            {
                if (c == '<') angleDepth++;
                else if (c == '>') angleDepth--;
                else if (c == '(') parenDepth++;
                else if (c == ')') parenDepth--;
                else if (c == '{') braceDepth++;
                else if (c == '}') braceDepth--;
            }

            if ((atEnd || c == ',') && angleDepth == 0 && parenDepth == 0 && braceDepth == 0)
            {
                string item = enumBody.Substring(start, i - start).Trim();
                start = i + 1;

                if (item.Length == 0)
                    continue;

                int eq = item.IndexOf('=');
                if (eq >= 0)
                    item = item.Substring(0, eq).Trim();

                Match nameMatch = Regex.Match(item, @"^(?<name>[A-Za-z_]\w*)$");
                if (nameMatch.Success)
                    values.Add(nameMatch.Groups["name"].Value);
            }
        }

        return values;
    }

    static string RemoveComments(string code)
    {
        StringBuilder result = new StringBuilder(code.Length);

        bool inLineComment = false;
        bool inBlockComment = false;
        bool inString = false;
        bool inChar = false;
        bool escape = false;

        for (int i = 0; i < code.Length; i++)
        {
            char c = code[i];
            char next = i + 1 < code.Length ? code[i + 1] : '\0';

            if (inLineComment)
            {
                if (c == '\r' || c == '\n')
                {
                    inLineComment = false;
                    result.Append(c);
                }
                continue;
            }

            if (inBlockComment)
            {
                if (c == '*' && next == '/')
                {
                    inBlockComment = false;
                    i++;
                }
                else if (c == '\r' || c == '\n')
                {
                    result.Append(c);
                }
                continue;
            }

            if (inString)
            {
                result.Append(c);

                if (escape)
                    escape = false;
                else if (c == '\\')
                    escape = true;
                else if (c == '"')
                    inString = false;
                continue;
            }

            if (inChar)
            {
                result.Append(c);

                if (escape)
                    escape = false;
                else if (c == '\\')
                    escape = true;
                else if (c == '\'')
                    inChar = false;
                continue;
            }

            if (c == '/' && next == '/')
            {
                inLineComment = true;
                i++;
                continue;
            }

            if (c == '/' && next == '*')
            {
                inBlockComment = true;
                i++;
                continue;
            }

            if (c == '"')
                inString = true;
            else if (c == '\'')
                inChar = true;

            result.Append(c);
        }

        return result.ToString();
    }

    static string EscapeForCppString(string value)
    {
        return value.Replace("\\", "\\\\").Replace("\"", "\\\"");
    }

    static string GetSourceRelativeInclude(string sourceDir, string file)
    {
        string prefix = sourceDir.TrimEnd(Path.DirectorySeparatorChar, Path.AltDirectorySeparatorChar) + Path.DirectorySeparatorChar;
        if (file.StartsWith(prefix, StringComparison.OrdinalIgnoreCase))
            return file.Substring(prefix.Length).Replace('\\', '/');
        return Path.GetFileName(file);
    }
    static bool TryExtractBraceBlock(string code, int searchStart, out int openBraceIndex, out int closeBraceIndex, out string body)
    {
        openBraceIndex = -1;
        closeBraceIndex = -1;
        body = "";

        openBraceIndex = code.IndexOf('{', searchStart);
        if (openBraceIndex < 0)
            return false;

        int depth = 0;
        bool inString = false;
        bool inChar = false;
        bool escape = false;

        for (int i = openBraceIndex; i < code.Length; i++)
        {
            char c = code[i];

            if (inString)
            {
                if (escape)
                    escape = false;
                else if (c == '\\')
                    escape = true;
                else if (c == '"')
                    inString = false;

                continue;
            }

            if (inChar)
            {
                if (escape)
                    escape = false;
                else if (c == '\\')
                    escape = true;
                else if (c == '\'')
                    inChar = false;

                continue;
            }

            if (c == '"')
            {
                inString = true;
                continue;
            }

            if (c == '\'')
            {
                inChar = true;
                continue;
            }

            if (c == '{')
            {
                depth++;
            }
            else if (c == '}')
            {
                depth--;

                if (depth == 0)
                {
                    closeBraceIndex = i;
                    body = code.Substring(openBraceIndex + 1, closeBraceIndex - openBraceIndex - 1);
                    return true;
                }
            }
        }

        return false;
    }

    static bool WriteAllTextIfChanged(string path, string content)
    {
        if (File.Exists(path))
        {
            string oldContent = File.ReadAllText(path);
            if (oldContent == content)
            {
                return false;
            }
        }

        File.WriteAllText(path, content);
        return true;
    }

    #endregion

    static void Main(string[] args)
    {
        Console.OutputEncoding = Encoding.UTF8;

        string currentDir = args.Length > 0 ? args[0] : Directory.GetCurrentDirectory();
        currentDir = Path.GetFullPath(currentDir);

        Console.WriteLine($"\n[UHT] Parser cwd: {currentDir}");

        string projectRootDir = currentDir;
        while (!Directory.Exists(Path.Combine(projectRootDir, "Source")))
        {
            DirectoryInfo parent = Directory.GetParent(projectRootDir);
            if (parent == null)
            {
                Console.WriteLine("[UHT][Error] Could not find Source directory.");
                return;
            }
            projectRootDir = parent.FullName;
        }

        string sourceDir = Path.Combine(projectRootDir, "Source");
        string outputDir = Path.Combine(projectRootDir, "Intermediate", "Generated");
        Directory.CreateDirectory(outputDir);

        Console.WriteLine($"[UHT] Project root: {projectRootDir}");
        Console.WriteLine($"[UHT] Source: {sourceDir}");
        Console.WriteLine($"[UHT] Output: {outputDir}");

        var headerFiles = Directory.GetFiles(sourceDir, "*.h", SearchOption.AllDirectories)
            .OrderBy(path => path, StringComparer.OrdinalIgnoreCase)
            .ToArray();
        int parsedCount = 0;
        int generatedWrittenCount = 0;
        int generatedUnchangedCount = 0;
        List<string> generatedCppFiles = new List<string>();

        foreach (var file in headerFiles)
        {
            if (file.EndsWith(".generated.h", StringComparison.OrdinalIgnoreCase))
                continue;

            bool bHasReflectionData = false;

            string cppCode = File.ReadAllText(file);
            string parseCode = RemoveComments(cppCode);

            MatchCollection classMatches = ClassRegex.Matches(parseCode);
            MatchCollection structMatches = StructRegex.Matches(parseCode);
            MatchCollection enumMatches = EnumRegex.Matches(parseCode);

            if (classMatches.Count == 0 && structMatches.Count == 0 && enumMatches.Count == 0)
                continue;

            string fileNameOnly = Path.GetFileNameWithoutExtension(file);
            string generatedHeaderPath = Path.Combine(outputDir, $"{fileNameOnly}.generated.h");
            string generatedCppPath = Path.Combine(outputDir, $"{fileNameOnly}.gen.cpp");
            string includePath = GetSourceRelativeInclude(sourceDir, file);

            StringBuilder headerContent = new StringBuilder();
            headerContent.AppendLine("// [UHT generated header - do not edit]");
            headerContent.AppendLine("#pragma once");
            headerContent.AppendLine();

            StringBuilder cppContent = new StringBuilder();
            cppContent.AppendLine("// [UHT generated source - do not edit]");
            cppContent.AppendLine($"#include \"{includePath}\"");
            cppContent.AppendLine("#include \"Core/ReflectionDatabase.h\"");
            cppContent.AppendLine("#include \"Core/ReflectionUtils.h\"");
            cppContent.AppendLine();

            // ---------------------------------------------------------
            // 1. USTRUCT 파싱 로직 (새로 추가됨!)
            // ---------------------------------------------------------
            foreach (Match match in structMatches)
            {
                bHasReflectionData = true;

                // 이름과 부모 추출 (부모가 없으면 빈 문자열)
                string structName = match.Groups["name"].Value;
                string parentName = match.Groups["parent"].Success ? match.Groups["parent"].Value : "";

                Console.WriteLine($"\n [Parsing Success] Find Structure: {structName} (FineName: {fileNameOnly}.h)");

                // 구조체용 GENERATED_BODY_ClassName (구조체는 virtual 함수가 필요 없음)
                headerContent.AppendLine($@"extern void Register_{structName}();
#undef GENERATED_BODY_{structName} // 매크로 충돌 방지를 위해 이름 뒤에 구조체명 붙임
#define GENERATED_BODY_{structName}() \
public: \
    friend void Register_{structName}(); \
    inline static FStructInfo StaticStructInfo;
");

                cppContent.AppendLine($@"void Register_{structName}() {{
    FStructInfo& info = {structName}::StaticStructInfo;

    info.StructName = ""{structName}"";
    info.Size = sizeof({structName});");
                cppContent.AppendLine("    info.Properties.clear();");
                cppContent.AppendLine("    info.ReflectedProperties.clear();");
                cppContent.AppendLine("    info.GcPointerOffsets.clear();");

                cppContent.AppendLine($"    info.ParentStructName = \"{EscapeForCppString(parentName)}\";");
                //if (!string.IsNullOrEmpty(parentName))
                //{
                //    cppContent.AppendLine($"    info.ParentStruct = ReflectionDatabase::GetStruct(\"{EscapeForCppString(parentName)}\");");
                //}

                // 구조체 내부 변수들(UPROPERTY) 찾기
                // (완벽한 파서를 원하신다면 괄호 { } 짝맞추기 로직이 추가되어야 합니다.)

                // 
                if (TryExtractBraceBlock(parseCode, match.Index, out _, out _, out string structBody))
                {
                    MatchCollection props = PropertyRegex.Matches(structBody);

                    // 기존 foreach (Match prop in props) 내부 코드를 이렇게 변경하세요:
                    foreach (Match prop in props)
                    {
                        string options = prop.Groups["options"].Value;
                        string type = prop.Groups["type"].Value.Trim();
                        string name = prop.Groups["name"].Value;
                        ParsedPropertyOptions parsedOptions = ParsePropertyOptions(options);

                        // ★ 새로운 FProperty 생성 코드 주입
                        string propCode = GeneratePropertyCode(type, name, structName, parsedOptions);
                        cppContent.AppendLine(propCode);

                        //// GC 추적용 포인터 기록 (선택 사항: 나중에 CollectReferences로 완벽히 대체되면 지우셔도 됩니다)
                        //if (type.Contains("*"))
                        //{
                        //    cppContent.AppendLine($"    info.GcPointerOffsets.push_back(offsetof({structName}, {name}));");
                        //}
                    }
                }


                cppContent.AppendLine($@"    ReflectionDatabase::AddStruct(""{structName}"", &info);
}}
struct FAutoRegister_{structName} {{ FAutoRegister_{structName}() {{ Register_{structName}(); }} }};
static FAutoRegister_{structName} AutoRegister_{structName}_Instance;
");
            }

            // ---------------------------------------------------------
            // 2. UENUM 파싱 로직 
            // ---------------------------------------------------------

            foreach (Match match in enumMatches)
            {
                bHasReflectionData = true;

                string enumName = match.Groups["name"].Value;

                cppContent.AppendLine($"static FEnumInfo StaticEnumInfo_{enumName};");

                cppContent.AppendLine($@"void Register_{enumName}() {{
    FEnumInfo& info = StaticEnumInfo_{enumName};
    info.EnumName = ""{EscapeForCppString(enumName)}"";
    info.Values.clear();
");

                if (TryExtractBraceBlock(parseCode, match.Index, out _, out _, out string enumBody))
                {
                    foreach (string valueName in ParseEnumValueNames(enumBody))
                    {
                        cppContent.AppendLine(
                            $"    info.Values.push_back({{ \"{EscapeForCppString(valueName)}\", static_cast<int64>({enumName}::{valueName}) }});");
                        cppContent.AppendLine(
    $"    info.CachedNames.push_back(ReflectionUtils::GetStablePropertyName(FName(\"{EscapeForCppString(valueName)}\")));");
                    }
                }

                cppContent.AppendLine($@"    ReflectionDatabase::AddEnum(""{EscapeForCppString(enumName)}"", &info);
}}

struct FAutoRegister_{enumName}
{{
    FAutoRegister_{enumName}() {{ Register_{enumName}(); }}
}};
static FAutoRegister_{enumName} AutoRegister_{enumName}_Instance;
");
            }

            // ---------------------------------------------------------
            // 3. UCLASS 파싱 로직 (기존과 거의 동일, 단일 매치 -> 다중 매치로 변경)
            // ---------------------------------------------------------
            for (int i = 0; i < classMatches.Count; i++)
            {
                bHasReflectionData = true;

                Match classMatch = classMatches[i];
                string className = classMatch.Groups["name"].Value;
                string parentName = classMatch.Groups["parent"].Value;

                //int classStart = classMatch.Index;
                //int classEnd = (i + 1 < classMatches.Count)
                //    ? classMatches[i + 1].Index
                //    : parseCode.Length;
                //string classBlock = parseCode.Substring(classStart, classEnd - classStart);

                if (!TryExtractBraceBlock(parseCode, classMatch.Index, out _, out _, out string classBlock))
                    continue;


                parsedCount++;
                Console.WriteLine($"[UHT] Class: {className} : {parentName} ({includePath})");

                headerContent.AppendLine($"extern void Register_{className}();");
                headerContent.AppendLine();
                headerContent.AppendLine($"#define GENERATED_BODY_{className}() \\");
                headerContent.AppendLine("public: \\");
                headerContent.AppendLine($"    friend void Register_{className}(); \\");
                headerContent.AppendLine("    inline static FClassInfo StaticClassInfo; \\");
                headerContent.AppendLine("    virtual FClassInfo* GetStaticClass() override { return &StaticClassInfo; }");
                headerContent.AppendLine();

                cppContent.AppendLine($"void Register_{className}()");
                cppContent.AppendLine("{");
                cppContent.AppendLine($"    FClassInfo& info = {className}::StaticClassInfo;");
                cppContent.AppendLine("    info.Properties.clear();");
                cppContent.AppendLine("    info.ReflectedProperties.clear();");
                cppContent.AppendLine("    info.GcPointerOffsets.clear();");
                cppContent.AppendLine($"    info.ClassName = \"{EscapeForCppString(className)}\";");
                cppContent.AppendLine($"    info.ParentClassName = \"{EscapeForCppString(parentName)}\";");
                //cppContent.AppendLine($"    info.ParentClass = ReflectionDatabase::GetClass(\"{EscapeForCppString(parentName)}\");");

                MatchCollection properties = PropertyRegex.Matches(classBlock);

                // 기존 foreach (Match prop in properties) 내부 코드를 이렇게 변경하세요:
                foreach (Match prop in properties)
                {
                    string options = prop.Groups["options"].Value;
                    string type = prop.Groups["type"].Value.Trim();
                    string name = prop.Groups["name"].Value;
                    ParsedPropertyOptions parsedOptions = ParsePropertyOptions(options);

                    Console.WriteLine($"   property: {type} {name} ({options})");

                    // ★ 새로운 FProperty 생성 코드 주입
                    string propCode = GeneratePropertyCode(type, name, className, parsedOptions);
                    cppContent.AppendLine(propCode);

                    //if (type.Contains("*"))
                    //    cppContent.AppendLine($"    info.GcPointerOffsets.push_back(offsetof({className}, {name}));");
                }

                cppContent.AppendLine($"    ReflectionDatabase::AddClass(\"{EscapeForCppString(className)}\", &info);");
                cppContent.AppendLine("}");
                cppContent.AppendLine();
                cppContent.AppendLine($"struct FAutoRegister_{className}");
                cppContent.AppendLine("{");
                cppContent.AppendLine($"    FAutoRegister_{className}() {{ Register_{className}(); }}");
                cppContent.AppendLine("};");
                cppContent.AppendLine($"static FAutoRegister_{className} AutoRegister_{className}_Instance;");
                cppContent.AppendLine();
            }
            if (bHasReflectionData)
            {
                if (WriteAllTextIfChanged(generatedHeaderPath, headerContent.ToString()))
                    generatedWrittenCount++;
                else
                    generatedUnchangedCount++;

                if (WriteAllTextIfChanged(generatedCppPath, cppContent.ToString()))
                    generatedWrittenCount++;
                else
                    generatedUnchangedCount++;

                generatedCppFiles.Add($"{fileNameOnly}.gen.cpp");
            }
        }

        string masterCppPath = Path.Combine(outputDir, "JSEngine.Reflection.gen.cpp");
        StringBuilder masterContent = new StringBuilder();
        masterContent.AppendLine("// [UHT generated master source - do not edit]");
        masterContent.AppendLine();

        generatedCppFiles.Sort(StringComparer.OrdinalIgnoreCase);
        foreach (var genFile in generatedCppFiles)
            masterContent.AppendLine($"#include \"{genFile}\"");

        if (WriteAllTextIfChanged(masterCppPath, masterContent.ToString()))
            generatedWrittenCount++;
        else
            generatedUnchangedCount++;

        Console.WriteLine($"\n[UHT] Generated master: {masterCppPath}");
        Console.WriteLine($"[UHT] Done. Parsed {parsedCount} classes. Written {generatedWrittenCount}, unchanged {generatedUnchangedCount}.");
    }
}
