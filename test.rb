require 'version_sorter'
require 'benchmark'

versions = File.readlines(File.expand_path('../test/tags.txt', __FILE__)).map(&:chomp)
versions = versions * 100;

Benchmark.bmbm do |x|
  x.report("sort") { VersionSorter.sort(versions) }
  x.report("sort_new") { VersionSorter.sort_(versions) }
end
