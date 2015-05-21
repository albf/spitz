/*
 * This file is part of spitz.
 *
 * spitz is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * spitz is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with spitz.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <cstdlib>
#include <iostream>
#include <spitz/spitz.hpp>

#define UNUSED(x) (void)(x)

class pijm : public spitz::jobmanager {
  private:
    long num_points;

  public:
    pijm(long num_points) : num_points(num_points) { }

    bool next_task(spitz::stream& stream) {
      if (num_points == 0)
        return false;
      double x = drand48();
      double y = drand48();
      stream << x << y;
      num_points--;
      return true;
    }
};

class piw : public spitz::worker {
  public:
    void run(spitz::stream& task, spitz::stream& result) {
      double x, y;
      task >> x >> y;
      result << (x*x + y*y <= 1);
    }
};

class pico : public spitz::committer {
  private:
    long num_points;
    long inside;

  public:
    pico(long num_points)
      : num_points(num_points)
      , inside(0) { }

    void commit_task(spitz::stream& result) {
      int i;
      result >> i;
      inside += i;
    }

    void commit_job(spitz::stream& final_result) {
      UNUSED(final_result);
      double pi = 4.0L * inside / num_points;
      std::cout << "pi from C++ program = " << pi << "\n";
    }
};

class pifactory : public spitz::factory {
  public:
    spitz::jobmanager *create_jobmanager(int argc, char **argv) {
      if (argc < 1)
        return 0;
      return new pijm(atol(argv[0]));
    }

    spitz::worker *create_worker(int argc, char **argv) {
      UNUSED(argc);
      UNUSED(argv);
      return new piw();
    }

    spitz::committer *create_committer(int argc, char **argv) {
      if (argc < 1)
        return 0;
      return new pico(atol(argv[0]));
    }
};

spitz::factory *spitz_factory = new pifactory();
