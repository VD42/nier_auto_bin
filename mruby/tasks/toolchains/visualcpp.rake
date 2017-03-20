MRuby::Toolchain.new(:visualcpp) do |conf|
  [conf.cc].each do |cc|
    cc.command = ENV['CC'] || 'C:\Program Files (x86)\Microsoft Visual Studio 14.0\VC\bin\cl.exe'
    # C4013: implicit function declaration
    cc.flags = [ENV['CFLAGS'] || %w(/c /nologo /W3 /we4013 /Zi /MD /O2 /D_CRT_SECURE_NO_WARNINGS /I"C:\Program Files (x86)\Microsoft Visual Studio 14.0\VC\include" /I"C:\Program Files (x86)\Windows Kits\10\Include\10.0.10240.0\ucrt" /I"c:\Program Files (x86)\Windows Kits\8.1\Include\um" /I"c:\Program Files (x86)\Windows Kits\8.1\Include\shared")]
    cc.defines = %w(DISABLE_GEMS MRB_STACK_EXTEND_DOUBLING)
    cc.option_include_path = '/I%s'
    cc.option_define = '/D%s'
    cc.compile_options = "%{flags} /Fo%{outfile} %{infile}"
  end

  [conf.cxx].each do |cxx|
    cxx.command = ENV['CXX'] || 'cl.exe'
    cxx.flags = [ENV['CXXFLAGS'] || ENV['CFLAGS'] || %w(/c /nologo /W3 /Zi /MD /O2 /EHs /D_CRT_SECURE_NO_WARNINGS)]
    cxx.defines = %w(DISABLE_GEMS MRB_STACK_EXTEND_DOUBLING)
    cxx.option_include_path = '/I%s'
    cxx.option_define = '/D%s'
    cxx.compile_options = "%{flags} /Fo%{outfile} %{infile}"
  end

  conf.linker do |linker|
    linker.command = ENV['LD'] || 'C:\Program Files (x86)\Microsoft Visual Studio 14.0\VC\bin\link.exe'
    linker.flags = [ENV['LDFLAGS'] || %w(/NOLOGO /DEBUG /INCREMENTAL:NO /OPT:ICF /OPT:REF /LIBPATH:"C:\Program Files (x86)\Microsoft Visual Studio 14.0\VC\lib" /LIBPATH:"C:\Program Files (x86)\Microsoft SDKs\Windows\v7.1A\Lib" /LIBPATH:"C:\Program Files (x86)\Windows Kits\10\Lib\10.0.14393.0\ucrt\x86")]
    linker.libraries = %w()
    linker.library_paths = %w()
    linker.option_library = '%s.lib'
    linker.option_library_path = '/LIBPATH:%s'
    linker.link_options = "%{flags} /OUT:%{outfile} %{objs} %{flags_before_libraries} %{libs} %{flags_after_libraries}"
  end

  conf.archiver do |archiver|
    archiver.command = ENV['AR'] || 'C:\Program Files (x86)\Microsoft Visual Studio 14.0\VC\bin\lib.exe'
    archiver.archive_options = '/nologo /OUT:%{outfile} %{objs}'
  end

  conf.yacc do |yacc|
    yacc.command = ENV['YACC'] || 'C:\HashiCorp\Vagrant\embedded\bin\bison.exe'
    yacc.compile_options = '-o %{outfile} %{infile}'
  end

  conf.gperf do |gperf|
    gperf.command = 'gperf.exe'
    gperf.compile_options = '-L ANSI-C -C -p -j1 -i 1 -g -o -t -N mrb_reserved_word -k"1,3,$" %{infile} > %{outfile}'
  end

  conf.exts do |exts|
    exts.object = '.obj'
    exts.executable = '.exe'
    exts.library = '.lib'
  end

  conf.file_separator = '\\'
end
