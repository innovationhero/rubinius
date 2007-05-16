ROOT = File.expand_path(File.dirname(__FILE__))

def require_files(files)
  files.each do |path|
    begin
      require(path)
    rescue Object => e
      STDERR.puts "Unable to load #{path}. #{e.message} (#{e.class})"
    end
  end
end

begin
  require 'spec/rake/spectask'
rescue LoadError
  raise <<-EOM
  Unable to load spec/rake/spectask. RSpec is a requirement to build Rubinius.
  Please install RSpec before building (http://rspec.rubyforge.org).
EOM
end

# specify the compiler to use
COMPILER="../bin/obsolete.rcompile"
# COMPILER="../shotgun/rubinius -c"

# require local rake libs
# doesn't do anything gracefully on load error (yet)
paths = Dir[ File.join(File.dirname(__FILE__), 'rake/*') ]
require_files(paths)

# By default, run all the specs and tests
task :default => :spec

desc 'Run all specs'
task :spec => 'spec:new'
#desc "Run all specs and tests."
#task :spec do
#  Rake::Task['spec:all'].invoke rescue got_error = true
#
#  raise "Spec or test failures." if got_error
#end

namespace :spec do
  desc "Run all specs and tests."
  task :all do
    Rake::Task['spec:only'].invoke  rescue got_error = true
    Rake::Task['test:all'].invoke   rescue got_error = true

    raise "Spec or test failures." if got_error
  end
  
  desc "Setup code-cache directory"
  task :setup => 'setup:code_cache'
  
  namespace :setup do
    desc "Setup code-cache directory"
    task :code_cache do
      Dir.mkdir "code-cache" unless File.exists?("code-cache")
      FileUtils.rm Dir.glob("code-cache/*")
    end
    
    task :subtend do
      Dir[File.join(File.dirname(__FILE__),"spec/subtend/**/Rakefile")].each do |rakefile|
        sh "rake -f #{rakefile}"
      end
    end
  end
  
  desc "Run only specs but not any tests."
  spec_targets = %w(language shotgun library core targets primitives)

  # Convenience method to run a single spec test
  spec_targets.each do |group|
    spec_files = Dir[ File.join(File.dirname(__FILE__),"spec/#{group}/*_spec.rb") ]
    GroupSpecTask.new(group)
    namespace group do
      spec_files.each do |file|
        SpecificGroupSpecTask.new(File.basename(file, '_spec.rb'), :core)
      end
    end
  end


  task :only => spec_targets.collect! { |g| 'spec:' << g }

  # experimental -- need to adjust exclusions depending on what your testing
  namespace :coverage do
    desc "Generate a coverage report for the library specs."
    GroupCoverageReport.new(:library)

    desc "Generate a coverage report for the core specs."
    GroupCoverageReport.new(:core)
  end
  
  # New runner
  desc 'Run all specs'
  task :new do
    system 'bin/specrunner spec' 
  end

  # Specdiffs to make it easier to see what your changes have affected :)
  desc 'Run specs and produce a diff against current base'
  task :diff => 'diff:run'

  namespace :diff do
    desc 'Run specs and produce a diff against current base'
    task :run do
      system 'bin/specrunner spec > /tmp/rbs_specdiff' 
      system 'diff -u spec/diffs/base.txt /tmp/rbs_specdiff'
      system 'rm /tmp/rbs_specdiff'
    end

    desc 'Replace the base spec file with a new one'
    task :replace do
      system 'bin/specrunner spec > spec/diffs/base.txt' 
    end
  end
end 

desc "Alias for test:all"
task :test => 'test:all'

namespace :test do
  desc "Run all tests."
  task :all do
    Rake::Task['test:core'].invoke    rescue got_error = true
    Rake::Task['test:shotgun'].invoke rescue got_error = true

    raise "Test failures." if got_error
  end

  desc "Runs BFTS on Ruby 1.8.*."
  task :bfts do
    system("ruby -Inative -Ibfts/overlay bfts/overlay/tc_all.rb")
  end

  desc "Runs part of BFTS under Rubinius natively."
  task :nativebfts => ['test:setup:code_cache'] do
    system("ruby -Ibfts/overlay bfts/overlay/test_stuff.rb")
  end

  desc "Run rubinius's 1.8.* tests."
  task :core do
    system("ruby -Ilib test/tc_all.rb")
  end

  desc "Run shotgun's core tests."
  task :shotgun => ['test:setup:code_cache'] do
    system("ruby test/tc_all.rb shotgun-tests")
  end

  namespace :setup do
    desc "Prepare the code-cache directory"
    task :code_cache => 'spec:setup:code_cache'
  end
end

desc "Build the whole project"
task :build => 'build:all'

desc "Rebuild all parts and archives from scratch."
task :rebuild => ['build:clean', 'build:delete_rbc', 'build:delete_rba', 'build:setup:syd', 
  'build:shotgun', 'build:bootstrap', 'build:core', 'build:library', 'build:compiler']

namespace :build do
  
  desc "Build rubinius (shotgun)."
  task :all => ['build:clean', 'build:shotgun']
  
  desc "Cleanup build files."
  task :clean do
    FileUtils.rm_rf 'code-cache'
    FileUtils.rm Dir.glob('lib/kernel.rb*')
    sh "(find native -name '*.rbc' | xargs rm) || true"
    Dir.chdir('shotgun') do
      sh 'make -e clean || true'
    end
  end
  
  desc "Rebuild librubinius"
  task :relib do
    sh 'make -C shotgun/lib clean'
    sh 'make'
  end
  
  task :configure do
    system('./configure')
    raise 'Failed to configure shotgun components' if $?.exitstatus != 0
  end

  task :setup do
    sh "ruby bin/rcc"
  end

  namespace :setup do
    desc "Ensure that the correct version of the sydparse gem is installed."
    task :syd do
      require 'rubygems'
      begin
        gem 'sydparse', '>= 1.2.1'
      rescue Gem::LoadError
        puts "\nYour system does not have the required sysparse gem installed...\n"
        Rake::Task['syd'].invoke
        raise "Gem 'sydparse' must be installed from externals/syd-parser/pkg directory. Then re-run 'rake build'."
      end
    end
  end

  desc "Build the kernel/hints file that is used to generate auto.c and auto.h"
  task :fields => ['setup:syd'] do
    $:.unshift "lib"
    require 'types'
    hfd = File.open("kernel/hints", "w")
    # Process types in alphabetical order to avoid file modifications
    types = Rubinius::Types.keys.sort_by {|k| k.to_s}
    types.each do |name|
      mod = Rubinius::Types[name]
      next if mod::TotalFields.size == 0
      sname = mod.name.split("::").last
      hfd.puts sname
      mod::TotalFields.each do |fel|
        if fel == :instance_variables
          fel = :__ivars__
        end
        hfd.puts fel
      end
      hfd.puts "!"
    end
    hfd.close
  end
  
  desc "Build shotgun C components."
  task :shotgun => ['build:setup', 'spec:setup:code_cache', 'build:configure'] do
    system("make -e -C shotgun rubinius")
    raise 'Failed to build shotgun components' if $?.exitstatus != 0
  end
  
  desc "Install rubinius"
  task :install => 'build:shotgun' do
    system("#{ENV['SUDO']} make install")
    raise 'Failed to install rubinius' if $?.exitstatus != 0
  end
  
  #FileList['lib/types/*.rb'].each do |src|
  #	file 'kernel/hints' => src
  #end
    
  file 'kernel/hints' do
    puts "Building hints and fields..."
    Rake::Task['build:fields'].invoke
  end
  
  def newer?(file, cmp)
    File.exists?(cmp) and File.mtime(cmp) >= File.mtime(file)
  end
  
  desc "Build the VM bootstrap archive."
  task :rcompile_bootstrap => 'kernel/hints' do
    Dir.chdir "kernel" do
      files = Dir["bootstrap/*.rb"].sort

      changed = []
      files.each do |file|
        cmp = "#{file}c"
        unless newer?(file, cmp) # File.exists?(cmp) and File.mtime(cmp) >= File.mtime(file)
          changed << cmp
          system "#{COMPILER} #{file}"
          raise "Failed to compile #{file}" if $?.exitstatus != 0
        end
        file << "c"
      end

      File.open(".load_order.txt","w") do |f|
        f.puts files.join("\n")
      end
    
      archive = "../runtime/bootstrap.rba"

      if File.exists? archive
        if changed.empty?
          puts "No kernel/bootstrap files to update."
        else
          system "zip -u #{archive} .load_order.txt #{changed.join(' ')}"
        end
      else
        system "zip #{archive} .load_order.txt #{files.join(' ')}"
      end
    end
  end
  
  def update_archive(files, archive, dir=nil, path_truc=false)
    archive = File.expand_path archive
    
    changed = []
    files.each do |file|
      cmp = "#{file}c"
      unless newer?(file, cmp)
        changed << cmp
        system "shotgun/rubinius compile #{file}"
      end
      file << "c"
    end
    
    curdir = Dir.getwd
    if dir
      Dir.chdir(dir)
      changed.map! { |f| f.gsub!(%r!^#{dir}/!, "") } if path_truc
    end
    
    File.open(".load_order.txt","w") do |f|
      f.puts files.join("\n")
    end

    if File.exists? archive
      if changed.empty?
        puts "No files to update."
      else
        system "zip -u #{archive} .load_order.txt #{changed.join(' ')}"
      end
    else
      system "zip #{archive} .load_order.txt #{files.join(' ')}"
    end
    
    Dir.chdir(curdir) if dir
  end

  task :rba => [:bootstrap, :core, :compiler]
  
  desc "Build the VM bootstrap archive with rubinius."
  task :bootstrap do
      archive = ENV['OUTPUT'] || "runtime/bootstrap.rba"
      files = Dir["kernel/bootstrap/*.rb"].sort
      temp_bs = "runtime/bootstrap.rba.old"
      system "cp runtime/bootstrap.rba #{temp_bs}"
      ENV['BOOTSTRAP'] = temp_bs
      update_archive files, archive # ,  "kernel"
      system "rm #{temp_bs}"
  end

  desc "Build the core classes and methods archive."
  task :rcompile_core => 'kernel/hints' do
    files = nil
    Dir.chdir("kernel") do
      files = Dir["core/*.rb"].sort
      files.delete "core/__loader.rb"

      files << "core/__loader.rb"
      
      changed = []
      files.each do |file|
        cmp = "#{file}c"
        unless newer?(file, cmp) # File.exists?(cmp) and File.mtime(cmp) >= File.mtime(file)
          changed << cmp
          system "#{COMPILER} #{file}"
          raise "Failed to compile #{file}" if $?.exitstatus != 0
        end
        file << "c"
      end

      File.open(".load_order.txt","w") do |f|
        f.puts files.join("\n")
      end
      
      archive = "../runtime/core.rba"

      if File.exists? archive
        if changed.empty?
          puts "No kernel/core files to update."
        else
          system "zip -u #{archive} .load_order.txt #{changed.join(' ')}"
        end
      else
        system "zip #{archive} .load_order.txt #{files.join(' ')}"
      end
    end
  end

  desc "Build the core classes and methods archive."
  task :core do
    files = nil
    files = Dir["kernel/core/*.rb"].sort
    files.delete "kernel/core/__loader.rb"

    files << "kernel/core/__loader.rb"
      
    temp_bs = "runtime/core.rba.old"
    system "cp runtime/core.rba #{temp_bs}"
    ENV['CORE'] = temp_bs
    
    archive = ENV['OUTPUT'] || "runtime/core.rba"
    
    update_archive files, archive # , "kernel"
    
    system "rm #{temp_bs}"
  end
  
  desc "Build the standard library."
  task :library do
    Dir.chdir('library') do
      files = Dir['**/*.rb']
      changed = []
      files.each do |file|
        cmp = "#{file}c"
        unless newer?(file, cmp)
          changed << cmp
          system "#{COMPILER} #{file}"
          raise "Failed to compile #{file}" if $?.exitstatus != 0
        end
        file << 'c'
      end
      
      archive = "../runtime/library.rba"
      
      if File.exists? archive
        if changed.empty?
          puts "No library files to update."
        else
          system "zip -u #{archive} #{changed.join(' ')}"
        end
      else
        system "zip #{archive} #{files.join(' ')}"
      end
    end
  end
  
  desc "Build syd-parser."
  task :syd do
    puts "Building externals/syd-parser gem...\n"
    Dir.chdir('externals/syd-parser')
    system("rake gem")
    raise 'Failed to build externals/syd-parser gem' if $?.exitstatus != 0
    puts "\nNow do 'gem install externals/syd-parser/pkg/*.gem' as your gem superuser.\n\n"
  end

  desc "Package up the compiler"
  task :compiler do
    files = %w! bytecode/compiler bytecode/assembler bytecode/encoder
      sexp/simple_processor translation/normalize translation/local_scoping
      sexp/composite_processor translation/states sexp/exceptions
      bytecode/primitive_names !

    paths = []

    files.each do |name|
      file = "#{name}.rb"
      dir = File.dirname(file)
      dest_dir = File.join("native", dir)
      path = File.expand_path File.join("lib", file)
      dest = File.join("native", file)
      FileUtils.mkdir_p dest_dir
      FileUtils.symlink path, dest rescue nil
      
      paths << dest
    end

    paths += %w!native/bytecode/rubinius.rb native/bytecode/system_hints.rb!
    
    update_archive paths, "runtime/compiler.rba", "native", true
  end
    
  desc "Builds shotgun, kernel, and bootstraps the compiler"
  task :rubinius => ['build:shotgun', 'build:bootstrap', 'build:core', 'build:library', 'build:compiler']

  desc "Delete all cached .rbc files"
  task :delete_rbc do
    FileList['**/*.rbc', '*.rbc', '/tmp/*.rbc'].each do |fn|
      FileUtils.rm fn rescue nil
    end
  end
  
  desc "Delete all compiled ruby archives"
  task :delete_rba do
    FileList['runtime/*.rba'].each do |fn|
      FileUtils.rm fn rescue nil
    end
  end
end

namespace :rbx do
  desc "Clean build products and all compiled ruby files"
  task :clean => ['build:clean', 'build:delete_rbc', 'build:delete_rba']
  
  desc "Build all components that need it, but don't clean first"
  task :build do
    ::COMPILER = 'rbx -c'
    Rake::Task['build:all'].invoke
  end
  
  desc "Rebuilds everything after clean"
  task :rebuild => ['rbx:clean', 'rbx:build', 'build:bootstrap', 
    'build:core', 'build:library', 'build:compiler']
  
  task :install => ['rbx:build', 'build:install']
end
  
desc "Remove runtime/*.rba then svn up"
task :svn => 'svn:up'
namespace :svn do
  desc "Remove runtime/*.rba then svn up"
  task :up do
    FileList['runtime/*.rba'].each do |f|
      FileUtils.rm f
    end
    puts `svn up`
    
  end
end

task :report => 'report:all' # default
namespace :report do
  desc "Build all reports"
  task :all => [:completeness, :rspec_errors]

  desc "Build completeness report"
  task :completeness do
    run_report(:completeness)
  end

  desc "Build rspec errors report"
  task :rspec_errors do
    run_report(:rspec_errors)
  end

  desc "Build self-hosting errors report"
  task :self_hosting_errors do
    run_report(:self_hosting_errors)
  end

  def run_report(name)
    report = "#{ROOT}/bin/reports/#{name}.rb"
    dest   = "#{ROOT}/reports/#{name}.html"
    File.open("#{ROOT}/reports/#{name}.html", 'w') do |f|
      f.write `RUBYOPT='' ruby #{report}`
    end
    puts "ruby #{report} > #{dest}"
  end
end

desc "Run Ruby 1.9.x benchmark suite"
task :bm do
  sh "rbx benchmark/run.rb"
end
desc "Run Ruby 1.9.x benchmark suite"
task :benchmark => :bm

namespace :doc do
  desc "Learn how to contribute."
  task :contrib => 'doc:contrib:easy'

  namespace :contrib do
    desc "Find out about easy ways to contribute."
    task :easy do
      puts <<-EOM
      
    The Rubinius team welcomes contributions, bug reports, test cases, and monetary support.
    One possible way to help is:
    1. Visit the wiki at http://rubini.us
    2. Read up on writing RSpec specs for Rubinius.
    3. Read up on implementing the Ruby core library.
    4. Check if a spec exists for the method you are adding.
    5. Red, green, refactor.
    6. Submit a nice diff to the mailing list: http://groups.google.com/group/rubinius-devel
    7. When your patch is accepted, ask Evan for a commit bit.

    The 'ri' command is a rich source of examples and test cases.
    
    There are a lot of folks working, so pop into #rubinius channel on freenode and ask
    questions. Try to follow the conventions in the code and restrain impulses to go
    mad refactoring and reorganizing until you are well familiar with the code and have
    asked other's opinions. In the words of the venerable cabo: 'Primum non nocere'
    
    Thanks for helping to spread good will among all beings. Go on, start coding!
    
    EOM
    end

    desc "Find out about ways to contribute that may require a lot of knowledge or work."
    task :hard do
      puts "More to come. For now, just make everything work faster than anything else in the world."
    end
  end
end

namespace :svk do
  task :push do
    sh 'svk push --verbatim'
  end
end

# Deprecated rake tasks
def deprecate(msg)
  puts "Deprecated. Please use #{msg}."
end

desc "DEPRECATED: Run shotgun's core tests"
task :test_shotgun do
  deprecate 'test:shotgun'
  Rake::Task['test:shotgun'].invoke
end

desc "DEPRECATED: Run all the tests"
task :test_all do
  deprecate 'test:all'
  Rake::Task['test:all'].invoke
end

# This forces ruby inline to build everything in the 
# right place.
task :setup do
  deprecate 'build:setup'
  Rake::Task['build:setup'].invoke
end

desc "DEPRECATED: Build shotgun C components"
task :shotgun do
  deprecate 'build:shotgun'
  Rake::Task['build:shotgun'].invoke
end

desc "DEPRECATED: Prepare the code-cache directory"
task :code_cache do
  deprecate 'test:setup:code_cache'
  Rake::Task['test:setup:code_cache'].invoke
end

desc "DEPRECATED: Ensure that the correct version of the sydparse gem is installed."
task :ensure_syd do
  deprecate 'build:setup:syd'
  Rake::Task['build:setup:syd'].invoke
end  

desc "DEPRECATED: Build syd-parser."
task :syd do
  deprecate 'build:syd'
  Rake::Task['build:syd'].invoke
end

task :fields do
  deprecate 'build:fields'
  Rake::Task['build:fields'].invoke
end

# Combine the separate .rb files in lib into a single kernel.rb
task :kernel do
  deprecate 'build:setup:core'
  Rake::Task['build:core'].invoke
end

# Build the kernel
task :bk do
  deprecate 'build:core'
  Rake::Task['build:core'].invoke
end

# Build rubinius vm (i.e. shotgun, kernel, compiler bootstrap)
task :br do
  deprecate 'build:rubinius'
  Rake::Task['build:rubinius'].invoke
end

task :clean do
  deprecate 'build:clean'
  Rake::Task['build:clean'].invoke
end

task :push do
  deprecate 'svk:push'
  Rake::Task['svk:push'].invoke
end

desc "DEPRECATED: Alias for doc:contrib:easy."
task :find_low_hanging_fruit do
  deprecate 'doc:contrib:easy'
  Rake::Task['doc:contrib:easy'].invoke
end

# vim: syntax=ruby shiftwidth=2
